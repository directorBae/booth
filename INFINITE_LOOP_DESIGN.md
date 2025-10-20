# 무한 루프 재생 설계

## 핵심 설계 철학

### ❌ 재생 종료 감지 없음

- Parent는 Child의 재생 상태를 **모름**
- Parent는 재생 완료를 **감지하지 않음**
- Child는 재생 완료를 Parent에게 **알리지 않음**

### ✅ 무한 반복 재생

- **정지 버튼을 누르기 전까지** 계속 재생
- 동영상: 마지막 프레임 → 첫 프레임으로 자동 루프
- 이미지: 같은 프레임을 SYNC마다 계속 출력

---

## 🔄 전체 시스템 동작

### Parent의 역할

```
버튼 입력 → UART 명령 전송 → SYNC 펄스 무한 생성
                                ↓
                          44ms마다 반복
                                ↓
                        정지 버튼 누를 때까지
```

**Parent는 "재생 완료"라는 개념이 없습니다.**

- `isPlaying = true` 상태가 되면 SYNC 무한 생성
- `isPlaying = false`는 오직 정지 버튼으로만 가능

### Child의 역할

```
UART 명령 수신 → 파일 열기 → 프레임 읽기 → SYNC 대기 → LED 출력
                                ↓                        ↑
                          마지막 프레임?                  │
                                ↓                        │
                          첫 프레임으로 ────────────────────┘
                          (무한 반복)
```

**Child도 "재생 완료"라는 개념이 없습니다.**

- 마지막 프레임 후 자동으로 처음부터 재생
- 정지 명령(0x00)을 받을 때까지 무한 반복

---

## 📊 동작 시나리오

### 시나리오 1: 동영상 재생 (예: 100 프레임)

```
시간:  0ms ──> 4.4초 ──> 4.5초 ──> 8.8초 ──> 계속...

Parent: [버튼 1 누름]
        ↓
        UART: 0x01
        ↓
        SYNC 시작 (44ms마다)
        ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━→ 무한

Child:  파일 열기 (1_3.bin)
        ↓
        프레임 0 → SYNC → show()
        프레임 1 → SYNC → show()
        프레임 2 → SYNC → show()
        ...
        프레임 99 → SYNC → show()
        ↓
        프레임 0 → SYNC → show()  ← 자동 루프!
        프레임 1 → SYNC → show()
        ...
        (무한 반복)
```

**100 프레임 = 100 × 44ms = 4.4초**

- 4.4초마다 처음부터 다시 재생
- 정지 버튼 누를 때까지 무한 반복

### 시나리오 2: 이미지 재생 (1 프레임)

```
시간:  0ms ──> 44ms ──> 88ms ──> 132ms ──> 계속...

Parent: [버튼 2 누름]
        ↓
        UART: 0x02
        ↓
        SYNC 시작 (44ms마다)
        ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━→ 무한

Child:  파일 열기 (2_3.bin)
        ↓
        frameCount == 1 감지
        ↓
        isStaticImage = true
        ↓
        프레임 0 읽기 → SYNC 대기
        ↓ (44ms)
        SYNC → show()  ← 이미지 출력
        ↓
        프레임 0 다시 읽기 (seek to HEADER_SIZE)
        ↓ (88ms)
        SYNC → show()  ← 같은 이미지 출력
        ↓
        프레임 0 다시 읽기
        ↓ (132ms)
        SYNC → show()  ← 같은 이미지 출력
        ↓
        (무한 반복)
```

**이미지도 동영상과 동일하게:**

- 44ms마다 SYNC 신호 받음
- 매번 같은 프레임(0)을 다시 읽고 출력
- 정지 버튼 누를 때까지 계속

### 시나리오 3: 정지

```
시간:  0ms ──────────────> 정지 버튼 ──> 이후

Parent: [재생 중... SYNC 전송 중...]
        ↓
        정지 버튼 감지 (GPIO 26)
        ↓
        UART: 0x00
        ↓
        isPlaying = false
        ↓
        SYNC 전송 중지 ← 여기서 멈춤!

Child:  [재생 중...]
        ↓
        UART 수신: 0x00
        ↓
        stopPlayback()
        - SD 파일 닫기
        - FastLED.clear()
        - isPlaying = false
        ↓
        대기 상태
```

---

## 🔍 코드 분석

### Parent: 무한 SYNC 생성

```cpp
void loop() {
    checkButtons();

    if (isPlaying) {  // ← 이 조건이 true인 동안
        unsigned long now = millis();
        if (now - lastFrameTime >= 44) {
            sendSyncSignal();      // ← 계속 호출됨
            lastFrameTime = now;
        }
    }
    else {
        delay(10);  // ← 정지 상태에서만 대기
    }
}
```

**isPlaying이 true가 되는 경우:**

- 버튼 1-4 누름 → `startPlayback()` → `isPlaying = true`

**isPlaying이 false가 되는 경우:**

- 정지 버튼 누름 → `stopPlayback()` → `isPlaying = false`

**재생 완료로는 절대 false가 안 됨!**

### Child: 무한 루프 읽기

```cpp
void readNextFrame() {
    if (!sdFile || !isPlaying)
        return;

    // 루프 재생 (동영상/이미지 모두)
    if (currentFrame >= fileHeader.frameCount) {
        currentFrame = 0;                // ← 처음으로
        sdFile.seek(HEADER_SIZE);        // ← 파일 포인터 리셋

        if (!isStaticImage) {
            Serial.printf("\n[LOOP] Restarting...\n");
        }
    }

    // 프레임 읽기 (4320 bytes)
    sdFile.read(frameBuffer, STRIP_BYTES);

    // BGR → RGB 변환
    // ...

    currentFrame++;  // ← 다음 프레임
}
```

**동영상 (frameCount > 1):**

```
currentFrame: 0 → 1 → 2 → ... → 99 → 0 → 1 → 2 → ...
                                  ↑ 여기서 seek
```

**이미지 (frameCount == 1):**

```
currentFrame: 0 → 0 → 0 → 0 → 0 → ...
                  ↑ 매번 seek
```

---

## ⚙️ 수정 사항 요약

### 제거된 기능

```diff
- unsigned long imageDisplayStartTime = 0;
- const unsigned long IMAGE_DISPLAY_DURATION = 10000;
-
- // 10초 타이머 체크
- if (isStaticImage && isPlaying) {
-     if (millis() - imageDisplayStartTime >= 10000) {
-         stopPlayback();  // 자동 종료
-     }
- }
```

### 변경된 로직

#### loop() 함수

```diff
  void loop() {
      // UART 수신
      if (SerialUART.available()) {
          handleCommand(command);
      }

-     // 이미지 모드: 10초 후 종료
-     if (isStaticImage && isPlaying) {
-         if (now - imageDisplayStartTime >= 10000) {
-             stopPlayback();
-         }
-         return;  // 프레임 읽기 스킵
-     }

-     // 동영상 모드만 프레임 읽기
-     if (frameReady && isPlaying && !isStaticImage) {
+     // 모든 모드 프레임 읽기
+     if (frameReady && isPlaying) {
          readNextFrame();
          frameReady = false;
      }

-     // SYNC (동영상만)
-     if (syncReceived && !isStaticImage) {
+     // SYNC (모든 모드)
+     if (syncReceived) {
          FastLED.show();
          syncReceived = false;
          frameReady = true;
      }
  }
```

#### startPlayback() 함수

```diff
  if (fileHeader.frameCount == 1) {
      isStaticImage = true;
-     imageDisplayStartTime = millis();
      Serial.println("Static image detected");
-     Serial.println("Will be displayed for 10 seconds");
+     Serial.println("Will be displayed continuously");

-     // 즉시 표시
      readNextFrame();
-     FastLED.show();
+     frameReady = false;  // SYNC 대기
  }
```

---

## 🎯 동작 원리 정리

### 핵심 원칙

1. **Parent는 타이밍만 제공** (재생 상태 모름)
2. **Child는 독립적으로 반복** (종료 신호 안 보냄)
3. **정지는 사용자가 직접** (자동 종료 없음)

### 데이터 흐름

```
[사용자] ──버튼──> [Parent] ──UART──> [Child]
                      │                  │
                      └──SYNC──> [Child] │
                                         │
                                    [무한 반복]
                                         │
                                         ↓
                                    [LED 출력]
```

### 종료 조건

**유일한 종료 방법: 정지 버튼**

```
사용자 → 정지 버튼 → Parent UART(0x00) → Child stopPlayback()
                  → Parent SYNC 중지
```

---

## 💡 왜 이런 설계인가?

### 장점

1. **단순성**: Parent는 재생 완료를 신경 쓸 필요 없음
2. **독립성**: Child들이 각자 속도로 루프 재생
3. **안정성**: UART 단방향 통신으로 충분
4. **확장성**: Child 개수와 무관하게 동작

### 단점

1. **재생 완료 감지 불가**: Parent가 언제 끝났는지 모름
2. **배터리 소모**: 무한 반복으로 전력 소비

### 해결책

- **사용자가 직접 제어**: 원하는 만큼 재생 후 정지 버튼
- **전력 관리**: 필요 시 밝기 조정 (`FastLED.setBrightness()`)

---

## 📝 테스트 체크리스트

### 동영상 테스트

- [ ] 마지막 프레임 → 첫 프레임으로 매끄럽게 전환
- [ ] 여러 번 루프해도 깨짐 없음
- [ ] 10개 Child 모두 동기화 유지

### 이미지 테스트

- [ ] 이미지가 계속 표시됨 (꺼지지 않음)
- [ ] SYNC 신호마다 정상 출력
- [ ] 정지 버튼으로 즉시 종료

### 전환 테스트

- [ ] 동영상 → 이미지 전환 정상
- [ ] 이미지 → 동영상 전환 정상
- [ ] 동영상 → 다른 동영상 전환 정상

### 정지 테스트

- [ ] 재생 중 정지 버튼 → 즉시 종료
- [ ] LED 완전히 꺼짐
- [ ] 다시 재생 시 정상 동작

---

## 🚀 결론

**시스템은 "무한 재생"을 기본으로 설계되었습니다.**

- Parent: 명령 + SYNC 무한 생성
- Child: 파일 무한 루프 재생
- 종료: 사용자가 정지 버튼으로 직접 제어

**이미지도 동영상과 동일하게 무한 반복!**
