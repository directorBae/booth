# 재생 시나리오 및 동작 원리

## 📌 질문 1: 1번 재생 중 2번 버튼을 누르면?

### ❌ 이전 동작 (수정 전)

```
1번 재생 중 → 2번 버튼 → 바로 2번으로 전환
문제: 1번 파일이 정상적으로 닫히지 않음
```

### ✅ 수정된 동작

```
1번 재생 중 → 2번 버튼 → 1번 정지 → 2번 시작
```

#### Parent 동작

```cpp
void startPlayback(uint8_t fileNum) {  // fileNum = 2
    if (isPlaying && currentFile == fileNum) {
        return;  // 이미 같은 파일 재생 중이면 무시
    }

    currentFile = 2;
    isPlaying = true;

    // UART로 명령 전송 (0x02)
    SerialUART.write(2);
}
```

**Parent는 단순히 새 명령(2)만 전송합니다.**

- 정지 신호를 별도로 보내지 않음
- SYNC 신호는 계속 전송 중 (재생 상태이므로)

#### Child 동작

```cpp
void handleCommand(uint8_t command) {  // command = 2
    if (command >= 1 && command <= 4) {
        // 다른 파일 재생 중이면 먼저 정지
        if (isPlaying && currentFileNum != command) {
            // currentFileNum = 1, command = 2 → 다름!
            Serial.printf("\n[SWITCH] Stopping file %d before playing file %d\n",
                         1, 2);
            stopPlayback();  // 1번 파일 닫기, LED 끄기
            delay(50);       // 안정화 대기
        }

        startPlayback(2);  // 2번 파일 열기
    }
}
```

**Child는 자동으로 전환을 처리합니다:**

1. 현재 재생 중인지 확인 (`isPlaying`)
2. 다른 파일인지 확인 (`currentFileNum != command`)
3. 다르면 → `stopPlayback()` 호출
   - SD 파일 닫기
   - LED 끄기
   - 상태 초기화
4. 50ms 대기 (SD 카드 안정화)
5. 새 파일(`2_X.bin`) 열기

#### 타임라인

```
시간:  0ms ──────> 50ms ──────> 100ms ──────> 144ms ──────>

Parent: [1번 재생 중... SYNC 전송 중...]
        ↓
        2번 버튼 감지
        ↓
        UART 전송: 0x02
        ↓
        [계속 SYNC 전송...]

Child:  [1번 재생 중... 1_3.bin 읽는 중...]
        ↓
        UART 수신: 0x02
        ↓
        1번 != 2번 → stopPlayback()
        - 1_3.bin 닫기
        - LED 끄기
        ↓
        50ms 대기
        ↓
        startPlayback(2)
        - 2_3.bin 열기
        - 헤더 읽기
        - 첫 프레임 준비
        ↓
        [2번 재생 시작... SYNC 대기...]
```

---

## 📌 질문 2: 동기화 텀은?

### ✅ 답: 매 프레임마다 동기화

#### Parent의 SYNC 신호 생성

```cpp
const unsigned long FRAME_DELAY_MS = 44;  // 약 22 FPS

void loop() {
    if (isPlaying) {
        unsigned long now = millis();
        if (now - lastFrameTime >= 44) {  // 44ms마다
            sendSyncSignal();  // GPIO 0: HIGH → LOW 펄스
            lastFrameTime = now;
        }
    }
}
```

**주기:**

- **44ms = 약 22.7 FPS**
- 1000ms ÷ 44ms = 22.7 프레임/초

#### Child의 SYNC 수신 및 출력

```cpp
void loop() {
    // [1] 프레임 읽기
    if (frameReady && isPlaying && !isStaticImage) {
        readNextFrame();  // SD에서 4320 bytes 읽기
        frameReady = false;
    }

    // [2] SYNC 신호 대기
    if (syncReceived && !isStaticImage) {
        FastLED.show();    // ← LED 출력!
        syncReceived = false;
        frameReady = true;  // 다음 프레임 읽기
    }
}

// 인터럽트 핸들러
void IRAM_ATTR onSyncInterrupt() {
    syncReceived = true;  // 플래그 설정
}
```

**동작:**

1. `readNextFrame()`: SD에서 프레임 데이터 읽기 (약 10-20ms 소요)
2. 읽기 완료 → `frameReady = false` → **대기 상태**
3. Parent의 SYNC 펄스 수신 (44ms 후)
4. 인터럽트 발생 → `syncReceived = true`
5. `loop()`에서 `FastLED.show()` 호출 → **LED 출력**
6. `frameReady = true` → 다음 프레임 읽기 시작

### 동기화 타이밍 다이어그램

```
Parent SYNC:  ━┓━━━━━┓━━━━━┓━━━━━┓━━━━━┓
              0ms   44ms  88ms  132ms 176ms

Child #1:     프레임0 읽기
              ↓
              대기...
              ↓ (44ms)
              show() → LED 출력
              ↓
              프레임1 읽기
              ↓
              대기...
              ↓ (88ms)
              show() → LED 출력

Child #10:    프레임0 읽기
              ↓
              대기...
              ↓ (44ms)
              show() → LED 출력  ← Child #1과 동시!
              ↓
              프레임1 읽기
```

**핵심:**

- 모든 Child가 **정확히 같은 순간** (Parent SYNC 펄스 시점)에 `FastLED.show()` 호출
- SD 읽기 속도가 달라도 SYNC가 맞춰줌
- 프레임마다 동기화 = **완벽한 타이밍 일치**

---

## 🎬 전체 재생 흐름

### 1번 재생 → 2번 재생 시나리오

```
┌─────────────────────────────────────────────────────────┐
│ Parent                                                  │
├─────────────────────────────────────────────────────────┤
│ 버튼 1 누름                                              │
│  ↓                                                       │
│ UART 전송: 0x01                                          │
│  ↓                                                       │
│ SYNC 시작 (44ms 주기)                                    │
│  ┃ (계속 전송 중...)                                     │
│  ┃                                                       │
│ 버튼 2 누름 ← 사용자 입력                                │
│  ↓                                                       │
│ UART 전송: 0x02                                          │
│  ┃                                                       │
│ SYNC 계속 (중단 없음)                                    │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│ Child #3                                                │
├─────────────────────────────────────────────────────────┤
│ UART 수신: 0x01                                          │
│  ↓                                                       │
│ 1_3.bin 열기                                             │
│  ↓                                                       │
│ 프레임 0 읽기 → SYNC 대기 → show()                       │
│ 프레임 1 읽기 → SYNC 대기 → show()                       │
│ 프레임 2 읽기 → SYNC 대기 → show()                       │
│  ┃                                                       │
│ UART 수신: 0x02 ← 여기서 전환!                           │
│  ↓                                                       │
│ currentFileNum(1) != command(2) 감지                    │
│  ↓                                                       │
│ stopPlayback()                                          │
│  - 1_3.bin 닫기                                          │
│  - FastLED.clear()                                      │
│  - isPlaying = false                                    │
│  ↓                                                       │
│ delay(50ms) ← 안정화                                     │
│  ↓                                                       │
│ startPlayback(2)                                        │
│  - 2_3.bin 열기                                          │
│  - 헤더 읽기                                             │
│  - 첫 프레임 읽기                                        │
│  ↓                                                       │
│ 프레임 0 읽기 → SYNC 대기 → show()                       │
│ 프레임 1 읽기 → SYNC 대기 → show()                       │
│  ┃ (새 파일 재생 중...)                                  │
└─────────────────────────────────────────────────────────┘
```

---

## 🛑 정지 버튼 시나리오

```
┌─────────────────────────────────────────────────────────┐
│ Parent                                                  │
├─────────────────────────────────────────────────────────┤
│ 정지 버튼 (GPIO 26) 누름                                 │
│  ↓                                                       │
│ UART 전송: 0x00                                          │
│  ↓                                                       │
│ isPlaying = false                                       │
│  ↓                                                       │
│ SYNC 전송 중지                                           │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│ Child                                                   │
├─────────────────────────────────────────────────────────┤
│ UART 수신: 0x00                                          │
│  ↓                                                       │
│ handleCommand(0) → stopPlayback()                       │
│  - SD 파일 닫기                                          │
│  - FastLED.clear()                                      │
│  - FastLED.show()                                       │
│  - isPlaying = false                                    │
│  ↓                                                       │
│ LED 꺼짐, 대기 상태                                      │
└─────────────────────────────────────────────────────────┘
```

---

## 📊 SYNC 신호 상세

### 타이밍

```
Parent GPIO 0:
    ┌─┐      ┌─┐      ┌─┐      ┌─┐
────┘ └──────┘ └──────┘ └──────┘ └────
    100μs    44ms     44ms     44ms

Child GPIO 0 인터럽트:
    ↑        ↑        ↑        ↑
    RISING   RISING   RISING   RISING
    EDGE     EDGE     EDGE     EDGE
```

### 펄스 폭

- **HIGH 구간**: 100μs
- **LOW 구간**: 43.9ms
- **주기**: 44ms (22.7 FPS)

### 왜 매 프레임마다?

1. **정확한 동기화**: 10개의 Child가 정확히 같은 순간에 출력
2. **SD 속도 차이 보정**: 느린 SD 카드도 SYNC 맞춰서 출력
3. **프레임 드랍 방지**: SYNC가 페이스메이커 역할

---

## 🔍 특수 케이스

### 케이스 1: 같은 버튼 연속 누름

```
1번 재생 중 → 1번 버튼 다시 누름 → 무시됨

void startPlayback(uint8_t fileNum) {
    if (isPlaying && currentFile == fileNum) {
        return;  // ← 여기서 종료
    }
}
```

### 케이스 2: 정적 이미지 (1 프레임)

```
frameCount == 1 감지
 ↓
isStaticImage = true
 ↓
프레임 읽기 → 즉시 FastLED.show()  (SYNC 무시)
 ↓
10초 타이머 시작
 ↓
10초 후 자동 stopPlayback()
```

**이미지 모드에서는 SYNC 신호를 받아도 무시:**

```cpp
if (syncReceived && !isStaticImage) {  // ← isStaticImage면 실행 안 됨
    FastLED.show();
    ...
}
```

### 케이스 3: SD 카드 읽기 실패

```
readNextFrame() 실패
 ↓
stopPlayback() 자동 호출
 ↓
LED 꺼짐, 대기 상태
```

---

## 📝 정리

| 항목                 | 동작                                           |
| -------------------- | ---------------------------------------------- |
| **SYNC 주기**        | 44ms (약 22 FPS)                               |
| **동기화 단위**      | 매 프레임마다                                  |
| **파일 전환**        | 자동 정지 → 새 파일 시작 (50ms 지연)           |
| **같은 파일 재선택** | 무시됨 (계속 재생)                             |
| **정지 명령**        | 즉시 정지, LED 끄기                            |
| **이미지 모드**      | SYNC 무시, 10초 후 자동 종료                   |
| **Parent 역할**      | 명령 전송 + SYNC 생성만 (재생 완료 감지 안 함) |
| **Child 역할**       | 독립적 재생 + SYNC 동기화                      |

**핵심:** Parent는 박자만 제공, Child들이 알아서 동기화해서 연주!
