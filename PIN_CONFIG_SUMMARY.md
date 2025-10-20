# 핀 구성 요약 - Multi-ESP32 LED System

## 시스템 개요

- **Parent ESP32-C5**: 1개 (컨트롤러 전용)
- **Child ESP32-C5**: 10개 (각각 SD + LED)
- **총 LED**: 14,400개 (180×80 매트릭스)
- **각 Child LED**: 1440개 (180×8)

---

## Parent ESP32-C5 핀 구성 (컨트롤러 전용)

### ⚠️ Parent에는 SD 카드와 LED Strip이 없습니다!

| 기능          | GPIO | 설명                           |
| ------------- | ---- | ------------------------------ |
| **버튼 입력** |      |                                |
| Button 1      | 9    | 파일 1 재생 (풀업 저항)        |
| Button 2      | 10   | 파일 2 재생 (풀업 저항)        |
| Button 3      | 13   | 파일 3 재생 (풀업 저항)        |
| Button 4      | 14   | 파일 4 재생 (풀업 저항)        |
| Button Stop   | 26   | 재생 정지 (풀업 저항)          |
| **동기화**    |      |                                |
| SYNC OUT      | 0    | 모든 Child GPIO 0에 병렬 연결  |
| **UART**      |      |                                |
| TX            | 23   | 모든 Child GPIO 23에 병렬 연결 |
| **공통**      |      |                                |
| GND           | GND  | 모든 Child와 공통 연결         |

### Parent 역할

- 버튼 입력 감지
- UART로 재생 명령 전송 (파일 1,2,3,4 또는 정지)
- SYNC 신호 전송 (약 22Hz, 44ms 간격)
- **SD 카드 읽기 없음**
- **LED 출력 없음**

---

## Child ESP32-C5 핀 구성 (10개, 각각 동일)

| 기능         | GPIO | 설명                         |
| ------------ | ---- | ---------------------------- |
| **SD 카드**  |      |                              |
| SD MISO      | 1    | SD Card #N (각자 다른 파일!) |
| SD MOSI      | 6    |                              |
| SD SCK       | 7    |                              |
| SD CS        | 8    |                              |
| **LED 출력** |      |                              |
| LED DATA     | 5    | WS2812 Strip #N (1440 LEDs)  |
| **동기화**   |      |                              |
| SYNC IN      | 0    | Parent GPIO 0에서 수신       |
| **UART**     |      |                              |
| RX           | 23   | Parent GPIO 23에서 수신      |
| **공통**     |      |                              |
| GND          | GND  | Parent와 공통 연결           |

### Child 역할

- UART로 재생 명령 수신
- SD 카드에서 프레임 데이터 읽기 (4320 bytes = 1440 LEDs × 3)
- LED 배열에 데이터 복사
- SYNC 신호 대기
- SYNC 수신 시 FastLED.show() 실행

---

## 연결 다이어그램

```
┌──────────────────┐
│ Parent ESP32-C5  │
│ (컨트롤러 전용)   │
│                  │
│ 버튼: 9,10,13,14,26 │
│ SYNC: 0 ─────────┼───┐
│ TX: 23 ──────────┼───┼───┐
│ GND ─────────────┼───┼───┼───┐
└──────────────────┘   ↓   ↓   ↓   ↓
        (SYNC, UART, GND 공유)
         ↓   ↓   ↓   ↓   ↓   ↓   ↓   ↓   ↓   ↓
    ┌────┼───┼───┼───┼───┼───┼───┼───┼───┼───┼────┐
    ↓    ↓   ↓   ↓   ↓   ↓   ↓   ↓   ↓   ↓   ↓    ↓
 [Child #1] [#2] [#3] [#4] [#5] [#6] [#7] [#8] [#9] [#10]
  SD #1    SD #2  SD #3  SD #4  SD #5  SD #6  SD #7  SD #8  SD #9  SD #10
  LED #1   LED #2 LED #3 LED #4 LED #5 LED #6 LED #7 LED #8 LED #9 LED #10
  (1440)   (1440) (1440) (1440) (1440) (1440) (1440) (1440) (1440) (1440)

  전체: 180×80 매트릭스 (14,400 LEDs)
```

---

## 코드 설정

### Parent 코드 예시

```cpp
// parent_controller.ino

// 버튼 핀
const int BUTTON_1 = 9;
const int BUTTON_2 = 10;
const int BUTTON_3 = 13;
const int BUTTON_4 = 14;
const int BUTTON_STOP = 26;

// 동기화 신호
const int SYNC_PIN = 0;

// UART
HardwareSerial Serial1(1);  // TX: GPIO 23

void setup() {
    // 버튼 핀 설정
    pinMode(BUTTON_1, INPUT_PULLUP);
    pinMode(BUTTON_2, INPUT_PULLUP);
    pinMode(BUTTON_3, INPUT_PULLUP);
    pinMode(BUTTON_4, INPUT_PULLUP);
    pinMode(BUTTON_STOP, INPUT_PULLUP);

    // SYNC 핀 설정
    pinMode(SYNC_PIN, OUTPUT);
    digitalWrite(SYNC_PIN, LOW);

    // UART 시작 (TX: 23)
    Serial1.begin(115200, SERIAL_8N1, -1, 23);  // RX 없음, TX만

    Serial.println("Parent controller ready");
}

void loop() {
    // 버튼 입력 처리
    if (digitalRead(BUTTON_1) == LOW) {
        Serial1.write(1);  // 파일 1 재생 명령
        startPlaying();
    }
    // ... 버튼 2, 3, 4, Stop 처리 ...

    // 재생 중이면 SYNC 신호 전송
    if (isPlaying) {
        delay(44);  // 22 FPS
        sendSyncSignal();
    }
}

void sendSyncSignal() {
    digitalWrite(SYNC_PIN, HIGH);
    delayMicroseconds(100);
    digitalWrite(SYNC_PIN, LOW);
}
```

### Child 코드 예시

```cpp
// child_player.ino

// SD 카드 핀
const int PIN_MISO = 1;
const int PIN_MOSI = 6;
const int PIN_SCK = 7;
const int PIN_CS = 8;

// LED 핀
const int PIN_LED = 5;

// 동기화 핀
const int PIN_SYNC = 0;

// UART 핀
const int PIN_RX = 23;

// Strip ID (각 Child마다 다름!)
const int STRIP_ID = 1;  // ← Child #1은 1, #2는 2, ..., #10은 10

// LED 설정
#define NUM_LEDS 1440
CRGB leds[NUM_LEDS];

volatile bool syncReceived = false;

void setup() {
    // SD 카드 초기화
    SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);
    if (!SD.begin(PIN_CS)) {
        Serial.println("SD Card failed!");
        while(1);
    }

    // FastLED 초기화
    FastLED.addLeds<WS2812, PIN_LED, GRB>(leds, NUM_LEDS);

    // SYNC 인터럽트
    pinMode(PIN_SYNC, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_SYNC), onSyncInterrupt, RISING);

    // UART 시작
    Serial1.begin(115200, SERIAL_8N1, PIN_RX, -1);  // RX만, TX 없음

    Serial.printf("Child #%d ready\n", STRIP_ID);
}

void onSyncInterrupt() {
    syncReceived = true;
}

void loop() {
    // UART 명령 수신
    if (Serial1.available()) {
        uint8_t command = Serial1.read();
        handleCommand(command);
    }

    // SD 프레임 읽기
    if (frameReady && isPlaying) {
        readNextFrame();  // 4320 bytes 읽기
        frameReady = false;
    }

    // SYNC 수신 시 LED 출력
    if (syncReceived) {
        FastLED.show();
        syncReceived = false;
        frameReady = true;
    }
}
```

---

## 주의사항

### ⚠️ 필수 확인사항

1. **Parent에는 SD 카드와 LED를 연결하지 마세요!**

   - Parent는 컨트롤러 전용
   - 버튼, SYNC, UART만 처리

2. **GND 공통 연결 필수!**

   - Parent GND와 모든 Child GND 연결
   - 안 하면 SYNC/UART 통신 불가

3. **SYNC와 UART는 1:N 병렬 연결**

   - Parent GPIO 0 → 모든 Child GPIO 0 (SYNC)
   - Parent GPIO 23 → 모든 Child GPIO 23 (UART)

4. **SD 카드 번호 일치**

   - Child #1 = SD #1 (strip_01.bin)
   - Child #2 = SD #2 (strip_02.bin)
   - ...
   - Child #10 = SD #10 (strip_10.bin)

5. **각 Child는 독립 전원**
   - 1440 LEDs = 최대 86A @ 100% 밝기
   - 10개 × 86A = 860A 필요

---

## 파일 준비

### 파일 분할 스크립트

```python
# split_video_file.py
import sys

def split_video_file(input_file, num_strips=10, leds_per_strip=1440):
    bytes_per_frame = num_strips * leds_per_strip * 3
    bytes_per_strip = leds_per_strip * 3  # 4320 bytes

    with open(input_file, 'rb') as f:
        # 헤더 읽기 (16 bytes)
        header = f.read(16)

        # 각 Strip용 파일 열기
        strip_files = []
        for i in range(num_strips):
            strip_file = open(f'strip_{i+1:02d}.bin', 'wb')
            strip_file.write(header)  # 헤더 복사
            strip_files.append(strip_file)

        # 프레임별로 분할
        while True:
            frame_data = f.read(bytes_per_frame)
            if len(frame_data) < bytes_per_frame:
                break

            for i in range(num_strips):
                start = i * bytes_per_strip
                end = start + bytes_per_strip
                strip_files[i].write(frame_data[start:end])

        # 파일 닫기
        for f in strip_files:
            f.close()

    print(f"Split completed: {num_strips} files created")

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python split_video_file.py input.bin")
        sys.exit(1)

    split_video_file(sys.argv[1])
```

### 사용법

```powershell
python split_video_file.py input_video.bin
```

출력: `strip_01.bin`, `strip_02.bin`, ..., `strip_10.bin`

---

## 성능

- **FPS**: 22+ FPS
- **총 LED**: 14,400개 (180×80)
- **병렬 처리**: 10개 Child 동시 SD 읽기 + LED 출력
- **동기화**: SYNC 신호로 마이크로초 단위 일치
- **단일 ESP32 대비**: 10배 빠름!

---

## 문의

문제가 발생하면 핀 연결을 다시 확인하세요!

- Parent: 버튼만 (SD/LED 없음!)
- Child: SD 카드 핀 (1,6,7,8) + LED 핀 (5)
- 공통: SYNC (0), UART (23), GND
