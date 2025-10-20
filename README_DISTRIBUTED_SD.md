# Multi-ESP32 LED System - 분산 SD 동기화 방식

## 시스템 개요

이 시스템은 10개의 ESP32-C5가 각자 독립적으로 SD 카드를 읽고 LED Strip을 제어하며, 1개의 Parent ESP32-C5가 **버튼 입력만 받아서** UART 명령 전송과 SYNC 신호로 모든 Child를 동기화하는 구조입니다.

**핵심 특징:**

- ✅ **22+ FPS** (단일 ESP32 대비 10배 향상!)
- ✅ **진짜 병렬 처리** (10개 Child가 동시에 SD 읽기 + LED 출력)
- ✅ **완벽한 동기화** (SYNC 신호로 마이크로초 단위 일치)
- ✅ **단순한 배선** (SYNC, UART, GND만 공유)
- ✅ **Parent는 컨트롤러 전용** (SD 카드 없음, LED 없음)
- ✅ **180×80 LED 매트릭스** (14,400 LEDs)

---

## 하드웨어 구성

- **Parent ESP32-C5**: 1개 (버튼 입력 + UART TX + SYNC 신호 송신만)
- **Child ESP32-C5**: 10개 (각각 SD 카드 + LED Strip 1440개 = 180×8)

### 시스템 구조

```
┌─────────────────────────────────────────────┐
│ Parent ESP32-C5 (컨트롤러 전용)              │
│  - 버튼 5개 (파일 1,2,3,4 재생 + 정지)       │
│  - SYNC 신호 전송 (모든 Child에게)           │
│  - UART TX (재생 명령 브로드캐스트)          │
│  ※ SD 카드 없음, LED 없음                   │
└─────────────────────────────────────────────┘
             ↓ (SYNC, UART, GND)
  ┌──────────┼──────────┬──────────┬──────────┐
  ↓          ↓          ↓          ↓          ↓
[Child #1] [Child #2] [Child #3] ... [Child #10]
 SD #1      SD #2      SD #3           SD #10
 LED #1     LED #2     LED #3          LED #10
 (1440)     (1440)     (1440)          (1440)

전체: 180×80 LED 매트릭스 (14,400 LEDs)
      10개 Child × 1440 LEDs (180×8 each)
```

---

## 핀 배치

### Parent ESP32-C5

| 기능                 | GPIO | 연결                                |
| -------------------- | ---- | ----------------------------------- |
| **버튼 입력**        |      |                                     |
| Button 1 (파일 1)    | 9    | 파일 1 재생 (풀업 저항 사용)        |
| Button 2 (파일 2)    | 10   | 파일 2 재생 (풀업 저항 사용)        |
| Button 3 (파일 3)    | 13   | 파일 3 재생 (풀업 저항 사용)        |
| Button 4 (파일 4)    | 14   | 파일 4 재생 (풀업 저항 사용)        |
| Button Stop          | 26   | 재생 정지 (풀업 저항 사용)          |
| **동기화 신호 출력** |      |                                     |
| SYNC OUT             | 0    | 모든 Child GPIO 0에 병렬 연결       |
| **UART (명령 전송)** |      |                                     |
| TX                   | 23   | 모든 Child GPIO 23 (RX)에 병렬 연결 |
| **공통**             |      |                                     |
| GND                  | GND  | 모든 Child GND에 공통 연결 (필수!)  |

**참고**: Parent는 SD 카드와 LED Strip이 **없습니다**. 버튼 입력만 받아서 명령을 전송하는 컨트롤러 역할만 합니다.

### Child ESP32-C5 (10개, 각각 동일 핀)

| 기능                 | GPIO | 연결                                    |
| -------------------- | ---- | --------------------------------------- |
| **SD 카드**          |      |                                         |
| SD MISO              | 1    | SD Card #N (각자 다른 데이터!)          |
| SD MOSI              | 6    |                                         |
| SD SCK               | 7    |                                         |
| SD CS                | 8    |                                         |
| **LED 출력**         |      |                                         |
| LED DATA             | 5    | WS2812 Strip #N DIN (1440 LEDs = 180×8) |
| **동기화 신호 입력** |      |                                         |
| SYNC IN              | 0    | Parent GPIO 0에서 수신                  |
| **UART (명령 수신)** |      |                                         |
| RX                   | 23   | Parent GPIO 23 (TX)에서 수신            |
| **공통**             |      |                                         |
| GND                  | GND  | Parent GND에 공통 연결 (필수!)          |

---

## 연결 다이어그램

### 공통 신호 연결 (Parent → 모든 Child)

```
ESP32-C5 Parent (컨트롤러)        ESP32-C5 Child (모두)
┌──────────┐                      ┌──────────┐
│ GPIO 0   │──────────┬───────────│ GPIO 0   │ (SYNC)
│          │          │           │          │
│ GPIO 23  │──────────┼───────────│ GPIO 23  │ (UART RX)
│          │          │           │          │
│ GND      │──────────┴───────────│ GND      │ (공통 GND)
└──────────┘                      └──────────┘

주의: SYNC와 UART는 1:N 연결
     (1개 신호선을 모든 Child에 병렬 연결)
     Parent에는 SD 카드와 LED가 없습니다!
```

### 개별 연결 (각 ESP32-C5)

#### Parent ESP32-C5 (컨트롤러 전용):

```
GPIO 9,10,13,14,26 → 버튼 5개 (풀업 저항 사용)
GPIO 0             → SYNC OUT (모든 Child GPIO 0에 병렬 연결)
GPIO 23            → TX (모든 Child GPIO 23에 병렬 연결)
GND                → 공통 GND (모든 Child와 연결)

※ SD 카드 없음
※ LED Strip 없음
```

#### Child ESP32-C5 (10개, 각각):

```
GPIO 1,6,7,8       → SD Card #N (각자 다른 SD 카드!)
GPIO 5             → LED Strip #N (1440 LEDs = 180×8)
GPIO 0             → SYNC IN (Parent GPIO 0에서 수신)
GPIO 23            → RX (Parent GPIO 23에서 수신)
GND                → 공통 GND (Parent와 연결)
```

## 주의사항

### ⚠️ 반드시 지켜야 할 사항

1. **모든 ESP32의 GND를 공통 연결** (필수! 안 하면 SYNC/UART 통신 불가)
2. **SYNC와 UART는 1:N 병렬 연결** (Parent 1개 신호선 → 모든 Child)
3. **각 Child는 독립된 5V 전원** (1440 LEDs = 최대 86A @ 100% 밝기)
4. **SD 카드는 각각 다른 데이터** (파일 분할 스크립트 사용)
5. **SD 카드 번호와 Child 번호 일치** (Child #1 = SD #1, Child #2 = SD #2, ...)
6. **Parent는 SD 카드와 LED 없음** (컨트롤러 전용)

### 📌 권장 사항

- SYNC와 UART 신호선은 짧게 (50cm 이내)
- SD 카드는 Class 10 이상 (UHS-I 권장)
- LED 전원은 Strip마다 독립적으로 공급 (10개 전원)
- 버튼은 디바운싱 처리 (하드웨어 또는 소프트웨어)
- LED 전원은 Strip마다 독립적으로 공급
- 버튼은 디바운싱 처리 (하드웨어 또는 소프트웨어)

---

## 예상 성능

### 기본 설정 (Class 10 SD, 9600 baud UART)

| 단계                           | 시간         | 계산                                                       |
| ------------------------------ | ------------ | ---------------------------------------------------------- |
| 1. 버튼 입력 처리 (Parent)     | <1ms         | GPIO 읽기 (무시 가능)                                      |
| 2. UART 명령 전송 (Parent)     | 0.8ms        | 1 byte × 8 bits / 9600 bps = 0.8ms                         |
| 3. SD 읽기 (병렬, 10개 Child)  | 1.73ms       | 4320 bytes × 8 bits / 20,000,000 bps = 1.73ms (모두 동시!) |
| 4. SYNC 신호 전송 (Parent)     | <1ms         | GPIO 신호 전파 (무시 가능)                                 |
| 5. LED 출력 (병렬, 10개 Child) | 43.2ms       | 1440 LEDs × 30μs (모두 동시!)                              |
| **총 프레임 시간**             | **~46ms**    |                                                            |
| **예상 FPS**                   | **21.7 FPS** | 🚀🚀                                                       |

### 최적화 설정 (UHS-I SD, 115200 baud UART)

| 단계                           | 시간         | 계산                                                       |
| ------------------------------ | ------------ | ---------------------------------------------------------- |
| 1. 버튼 입력 처리 (Parent)     | <1ms         | GPIO 읽기 (무시 가능)                                      |
| 2. UART 명령 전송 (Parent)     | 0.07ms       | 1 byte × 8 bits / 115200 bps = 0.07ms                      |
| 3. SD 읽기 (병렬, 10개 Child)  | 0.86ms       | 4320 bytes × 8 bits / 40,000,000 bps = 0.86ms (모두 동시!) |
| 4. SYNC 신호 전송 (Parent)     | <1ms         | GPIO 신호 전파 (무시 가능)                                 |
| 5. LED 출력 (병렬, 10개 Child) | 43.2ms       | 1440 LEDs × 30μs (모두 동시!)                              |
| **총 프레임 시간**             | **~44ms**    |                                                            |
| **예상 FPS**                   | **22.7 FPS** | 🚀🚀🚀                                                     |

### 성능 비교

| 구성                        | 총 LED     | FPS          | 비고                         |
| --------------------------- | ---------- | ------------ | ---------------------------- |
| ESP32-C5 단일               | 17,280     | 2.1 FPS      | 실제 측정값                  |
| 라즈베리파이                | 17,280     | 2-4 FPS      | 순차 전송                    |
| **ESP32 Multi (기본)**      | **14,400** | **21.7 FPS** | Class 10 SD + 9600 baud UART |
| **ESP32 Multi (최적화)** ⭐ | **14,400** | **22.7 FPS** | UHS-I SD + 115200 baud UART  |

**결론**:

- 14,400 LEDs (180×80) 기준 **22+ FPS 달성!**
- 단일 ESP32 17,280 LEDs 대비 **10배 빠름!** 🎯
- Parent는 컨트롤러만 담당, Child 10개가 병렬 재생

---

## 장점 vs 단점

### ✅ 장점

- **22+ FPS 달성** (단일 ESP32 대비 10배)
- **진짜 병렬 처리** (10개 Child가 동시에 SD 읽기 + LED 출력)
- **완벽한 동기화** (SYNC 신호로 마이크로초 단위 일치)
- **Parent는 컨트롤러 전용** (SD/LED 없이 버튼만 처리)
- **배선 단순** (SYNC, UART, GND 3개 신호선만 공유)
- **데이터 전송 없음** (각 Child가 독립적으로 SD 읽기)
- **확장 가능** (Child 추가/제거 쉬움)
- **180×80 매트릭스** (14,400 LEDs)

### ❌ 단점

- **하드웨어 복잡도 증가** (ESP32-C5 11개 필요: Parent 1개 + Child 10개)
- **SD 카드 10개 필요** (파일 분할 필수)
- **비용 증가** (ESP32-C5 × 11 + SD 카드 × 10)
- **전력 소모 증가** (Child 10개 + LED 14,400개)
- **파일 준비 과정 추가** (1개 파일 → 10개 분할)

---

## 소프트웨어 설정

### Parent ESP32-C5 (컨트롤러)

1. Arduino IDE 열기
2. 보드: **"ESP32C5 Dev Module"** 선택
3. `parent_controller/parent_controller.ino` 열기
4. 코드 확인:

   ```cpp
   // 버튼 핀
   const int BUTTON_1 = 9;
   const int BUTTON_2 = 10;
   const int BUTTON_3 = 13;
   const int BUTTON_4 = 14;
   const int BUTTON_STOP = 26;

   // 동기화 신호
   const int SYNC_PIN = 0;

   // UART TX
   const int TX_PIN = 23;  // Serial1
   ```

5. 업로드
6. **참고**: Parent는 SD 카드와 LED Strip 연결 불필요

### Child ESP32-C5 (10개)

각 Child마다 다음 과정 반복:

1. Arduino IDE 열기
2. 보드: **"ESP32C5 Dev Module"** 선택
3. `child_player/child_player.ino` 열기
4. **`STRIP_ID` 수정** (중요!):
   ```cpp
   const int STRIP_ID = 1;  // ← Child #1은 1, Child #2는 2, ..., Child #10은 10
   ```
5. SD 카드 핀 확인:
   ```cpp
   const int PIN_MISO = 1;
   const int PIN_MOSI = 6;
   const int PIN_SCK = 7;
   const int PIN_CS = 8;
   const int PIN_LED = 5;   // LED DATA 출력
   const int PIN_SYNC = 0;  // SYNC 입력
   const int PIN_RX = 23;   // UART RX
   ```
6. 업로드
7. SD 카드 #N에 Strip #N 데이터 저장
8. SD 카드를 Child #N에 삽입
9. 다음 Child로 이동, STRIP_ID 변경 후 반복
