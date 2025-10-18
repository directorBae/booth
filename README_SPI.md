# Multi-ESP32 LED System - SPI 방식

## 하드웨어 구성

- **Master**: ESP32-S3 (GPIO 0-21, 26-48 사용 가능)
- **Slave**: ESP32-C5 × 12 (GPIO 0-15, 23-28 사용 가능)

## 시스템 구조

```
Master ESP32-S3 (SD 카드 읽기 + 데이터 분배)
    │
    ├─ SPI Bus (공통 - 모든 Slave)
    │   ├─ MOSI (GPIO 11) ─────┬─ 모든 Slave MOSI (GPIO 11)
    │   ├─ SCK  (GPIO 12) ─────┼─ 모든 Slave SCK  (GPIO 12)
    │   └─ SYNC (GPIO 13) ─────┴─ 모든 Slave SYNC (GPIO 13)
    │
    ├─ CS Pins (개별 - 각 Slave 선택)
    │   ├─ CS0 (GPIO 1) ────── Slave #1 CS (GPIO 7)
    │   ├─ CS1 (GPIO 2) ────── Slave #2 CS (GPIO 7)
    │   ├─ CS2 (GPIO 3) ────── Slave #3 CS (GPIO 7)
    │   ├─ CS3 (GPIO 4) ────── Slave #4 CS (GPIO 7)
    │   ├─ CS4 (GPIO 5) ────── Slave #5 CS (GPIO 7)
    │   ├─ CS5 (GPIO 6) ────── Slave #6 CS (GPIO 7)
    │   ├─ CS6 (GPIO 7) ────── Slave #7 CS (GPIO 7)
    │   ├─ CS7 (GPIO 8) ────── Slave #8 CS (GPIO 7)
    │   ├─ CS8 (GPIO 9) ────── Slave #9 CS (GPIO 7)
    │   ├─ CS9 (GPIO 10) ───── Slave #10 CS (GPIO 7)
    │   ├─ CS10 (GPIO 14) ──── Slave #11 CS (GPIO 7)
    │   └─ CS11 (GPIO 15) ──── Slave #12 CS (GPIO 7)
    │
    └─ GND ─────────────────── 모든 Slave GND (공통)
```

## 핀 배치

### Master ESP32-S3

| 기능                     | GPIO                       | 용도                                 |
| ------------------------ | -------------------------- | ------------------------------------ |
| **SD 카드**              |                            |                                      |
| SD MISO                  | 37                         | SD 카드 데이터 입력                  |
| SD MOSI                  | 35                         | SD 카드 데이터 출력                  |
| SD SCK                   | 36                         | SD 카드 클럭                         |
| SD CS                    | 34                         | SD 카드 칩 선택                      |
| **버튼**                 |                            |                                      |
| Button 1                 | 38                         | 파일 1 재생                          |
| Button 2                 | 39                         | 파일 2 재생                          |
| Button 3                 | 40                         | 파일 3 재생                          |
| Button 4                 | 41                         | 파일 4 재생                          |
| Button Stop              | 42                         | 재생 정지                            |
| **SPI (Slave 통신)**     |                            |                                      |
| SPI MOSI                 | 11                         | Slave 데이터 전송                    |
| SPI SCK                  | 12                         | Slave 클럭                           |
| SPI SYNC                 | 13                         | 동기화 신호 (모든 Slave 동시 show()) |
| **CS Pins (Slave 선택)** |                            |                                      |
| CS[0-11]                 | 1,2,3,4,5,6,7,8,9,10,14,15 | 각 Slave 개별 선택                   |

### Slave ESP32-C5 (각각)

| 기능     | GPIO | 용도                               |
| -------- | ---- | ---------------------------------- |
| SPI MOSI | 11   | Master 데이터 수신                 |
| SPI SCK  | 12   | 클럭 수신                          |
| SPI CS   | 7    | 칩 선택 (Master의 CS_PINS[x] 연결) |
| SPI SYNC | 13   | 동기화 신호 수신                   |
| LED OUT  | 8    | WS2812 데이터 출력 (1440 LEDs)     |

## 하드웨어 연결 가이드

### 공통 연결 (Master → 모든 Slave)

```
ESP32-S3 Master → ESP32-C5 Slave (모두)
GPIO 11 → GPIO 11 (MOSI)
GPIO 12 → GPIO 12 (SCK)
GPIO 13 → GPIO 13 (SYNC)
GND     → GND
```

### 개별 CS 연결 (각 Slave)

```
Master CS → Slave CS
GPIO 1  → Slave #1 GPIO 7
GPIO 2  → Slave #2 GPIO 7
GPIO 3  → Slave #3 GPIO 7
GPIO 4  → Slave #4 GPIO 7
GPIO 5  → Slave #5 GPIO 7
GPIO 6  → Slave #6 GPIO 7
GPIO 7  → Slave #7 GPIO 7
GPIO 8  → Slave #8 GPIO 7
GPIO 9  → Slave #9 GPIO 7
GPIO 10 → Slave #10 GPIO 7
GPIO 14 → Slave #11 GPIO 7
GPIO 15 → Slave #12 GPIO 7
```

### LED 연결 (각 Slave)

```
Slave GPIO 8 → WS2812 Strip DIN (1440 LEDs)
```

## 연결 다이어그램 요약

```
[ESP32-S3 Master]
    SD Card: 34,35,36,37
    Buttons: 38,39,40,41,42
    SPI Out: 11(MOSI), 12(SCK), 13(SYNC)
    CS Out: 1,2,3,4,5,6,7,8,9,10,14,15
         ↓
    [공통 SPI Bus: 11,12,13,GND]
         ↓
    [ESP32-C5 Slave #1]     [ESP32-C5 Slave #2]     ...     [ESP32-C5 Slave #12]
    CS: 7 ← Master GPIO 1   CS: 7 ← Master GPIO 2           CS: 7 ← Master GPIO 15
    LED: 8 → Strip 1        LED: 8 → Strip 2                LED: 8 → Strip 12
    (1440 LEDs)             (1440 LEDs)                     (1440 LEDs)
```

## 주의사항

1. ⚠️ **모든 ESP32의 GND를 공통 연결** (필수! 안 하면 통신 불가)
2. ⚠️ **SPI 신호선은 짧게** (20cm 이내 권장, 긴 선은 노이즈 발생)
3. ⚠️ **각 Slave는 독립된 5V 전원** (1440 LEDs = 최대 86A @ 100% 밝기)
4. ⚠️ **ESP32-S3와 ESP32-C5는 다른 칩** (보드 설정 주의)

## 소프트웨어 설정

### Master ESP32-S3

1. Arduino IDE 열기
2. 보드: **"ESP32S3 Dev Module"** 선택
3. `master_spi/master_spi.ino` 열기
4. 업로드
5. SD 카드에 `.bin` 파일 저장 (`1.bin`, `2.bin`, `3.bin`, `4.bin`)

### Slave ESP32-C5 (각각 12개)

1. Arduino IDE 열기
2. 보드: **"ESP32C5 Dev Module"** 선택
3. `slave_spi/slave_spi.ino` 열기
4. **`SLAVE_ID` 수정** (중요!)
   ```cpp
   const int SLAVE_ID = 1; // ← Slave #1은 1, Slave #2는 2, ..., Slave #12는 12
   ```
5. 업로드
6. 다음 Slave로 이동, SLAVE_ID 변경 후 반복

## 예상 성능

| 항목                 | 시간          | 설명                               |
| -------------------- | ------------- | ---------------------------------- |
| SD 읽기              | 36ms          | 50KB @ 1400 KB/s                   |
| SPI 전송 (12 Slaves) | 2ms           | 4320 bytes × 12 @ 20MHz            |
| LED 출력 (병렬)      | 43ms          | 1440 LEDs × 30μs (모든 Slave 동시) |
| **총 프레임 시간**   | **~80ms**     | SD + SPI + LED                     |
| **예상 FPS**         | **12-15 FPS** | ✅                                 |

**비교:**

- 현재 단일 ESP32-C5: **2.1 FPS**
- Multi-ESP32 시스템: **12-15 FPS** (6-7배 향상!) 🚀

## 장점 vs 단점

### ✅ 장점

- **6-7배 빠른 FPS** (2.1 → 12-15 FPS)
- 병렬 LED 출력 (12개 Strip 동시 업데이트)
- 동기화된 화면 (SYNC 신호로 일치)
- 확장 가능 (Slave 추가/제거 쉬움)
- Master가 S3라 더 강력 (더 많은 GPIO, 빠른 CPU)

### ❌ 단점

- 하드웨어 복잡도 증가 (ESP32 13개)
- 배선 복잡 (SPI 공통 + CS 개별)
- 비용 증가 (ESP32-S3 × 1 + ESP32-C5 × 12)
- 전력 소모 증가
- 디버깅 어려움

## 테스트 순서

### 1단계: Master 단독 테스트

```
1. ESP32-S3에 master_spi.ino 업로드
2. 시리얼 모니터 확인 (SD 카드 인식 확인)
3. 버튼 동작 확인
```

### 2단계: Slave #1 추가

```
1. ESP32-C5에 slave_spi.ino 업로드 (SLAVE_ID=1)
2. Master + Slave #1 연결
   - 공통: MOSI(11), SCK(12), SYNC(13), GND
   - 개별: Master GPIO 1 → Slave GPIO 7
3. LED Strip 1 연결 (Slave GPIO 8)
4. 전원 on → 시리얼 모니터 확인
5. 버튼 눌러서 LED 동작 확인
```

### 3단계: Slave 추가

```
1. Slave #2 준비 (SLAVE_ID=2)
2. 연결 추가
3. 테스트
4. 반복... Slave #12까지
```

### 4단계: 전체 시스템 테스트

```
1. 12개 모두 연결
2. 파일 재생 (버튼 1-4)
3. FPS 확인 (시리얼 모니터)
4. 동기화 확인 (모든 Strip이 동일한 프레임 표시)
```

## 트러블슈팅

### 문제: Master가 SD 카드를 읽지 못함

- SD 카드 포맷 확인 (FAT32)
- 연결 확인 (34,35,36,37)
- SD 카드 속도 (Class 10 이상)

### 문제: Slave가 데이터를 받지 못함

- GND 공통 연결 확인 (가장 흔한 실수!)
- CS 연결 확인 (각 Slave마다 다른 Master GPIO)
- SPI 핀 연결 (11, 12, 13)
- 시리얼 모니터에서 에러 메시지 확인

### 문제: LED가 깜빡이거나 색상이 이상함

- 전원 부족 (5V 전원 용량 확인, 최소 100A 권장)
- SYNC 신호 확인
- FastLED 버전 확인 (최신 버전 사용)

### 문제: FPS가 낮음 (12 FPS 미만)

- SD 카드 속도 확인 (Class 10 → UHS-I)
- SPI 속도 높이기 (코드에서 20MHz → 40MHz)
- 시리얼 출력 줄이기 (출력 자체가 느림)
- 파일 크기 확인 (너무 큰 파일)

### 문제: 특정 Slave만 동작 안 함

- 해당 Slave의 SLAVE_ID 확인
- CS 연결 확인 (Master CS → Slave GPIO 7)
- 해당 Slave만 시리얼 모니터로 디버깅
- ESP32-C5 보드로 업로드했는지 확인

## 개발 팁

### 디버깅

- 각 Slave의 시리얼 모니터를 개별 확인
- Master는 프레임 전송 로그 출력
- Slave는 데이터 수신 로그 출력

### 최적화

- 불필요한 Serial.print 제거 (속도 향상)
- SPI 속도 조정 (안정성 vs 속도)
- SD 읽기 버퍼 크기 조정

### 확장

- Slave 추가 시 CS 핀만 추가
- Master의 CS_PINS 배열에 핀 번호 추가
- Slave 개수 (NUM_STRIPS) 증가
