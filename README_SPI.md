# Multi-ESP32 LED System - SPI 방식 (ESP32-C5)

## 시스템 구조

```
Master ESP32-C5 (SD 카드 읽기)
    │
    ├─ SPI Bus (공통)
    │   ├─ MOSI (GPIO 15) ─────┬─ 모든 Slave의 MOSI (GPIO 15)
    │   ├─ SCK  (GPIO 3) ──────┼─ 모든 Slave의 SCK  (GPIO 3)
    │   └─ SYNC (GPIO 16) ─────┴─ 모든 Slave의 SYNC (GPIO 16)
    │
    ├─ CS Pins (개별) - 각 Slave마다 다른 CS 핀에 연결
    │   ├─ CS0 (GPIO 1) ────── Slave #1 CS (GPIO 7)
    │   ├─ CS1 (GPIO 2) ────── Slave #2 CS (GPIO 7)
    │   ├─ CS2 (GPIO 25) ───── Slave #3 CS (GPIO 7)
    │   ├─ CS3 (GPIO 11) ───── Slave #4 CS (GPIO 7)
    │   ├─ CS4 (GPIO 24) ───── Slave #5 CS (GPIO 7)
    │   ├─ CS5 (GPIO 23) ───── Slave #6 CS (GPIO 7)
    │   ├─ CS6 (GPIO 0) ────── Slave #7 CS (GPIO 7)
    │   ├─ CS7 (GPIO 27) ───── Slave #8 CS (GPIO 7)
    │   ├─ CS8 (GPIO 4) ────── Slave #9 CS (GPIO 7)
    │   ├─ CS9 (GPIO 5) ────── Slave #10 CS (GPIO 7)
    │   ├─ CS10 (GPIO 28) ──── Slave #11 CS (GPIO 7)
    │   └─ CS11 (GPIO 12) ──── Slave #12 CS (GPIO 7)
    │
    └─ GND ─────────────────── 모든 Slave GND (공통)
```

## 핀 배치 요약

### Master ESP32-C5

| 기능                 | GPIO | 용도                            |
| -------------------- | ---- | ------------------------------- |
| **SD 카드**          |      |                                 |
| SD MISO              | 1    | SD 카드 데이터 입력 (CS와 공유) |
| SD MOSI              | 6    | SD 카드 데이터 출력             |
| SD SCK               | 7    | SD 카드 클럭                    |
| SD CS                | 8    | SD 카드 칩 선택                 |
| **버튼**             |      |                                 |
| Button 1             | 10   | 파일 1 재생                     |
| Button 2             | 9    | 파일 2 재생                     |
| Button 3             | 13   | 파일 3 재생                     |
| Button 4             | 14   | 파일 4 재생                     |
| Button Stop          | 26   | 재생 정지                       |
| **SPI (Slave 통신)** |      |                                 |
| SPI MOSI             | 15   | Slave 데이터 전송               |
| SPI SCK              | 3    | Slave 클럭                      |
| SPI SYNC             | 16   | 동기화 신호                     |
| **CS Pins**          |      |                                 |
| CS[0]                | 1    | Slave #1 선택 (SD MISO와 공유)  |
| CS[1]                | 2    | Slave #2 선택                   |
| CS[2]                | 25   | Slave #3 선택                   |
| CS[3]                | 11   | Slave #4 선택                   |
| CS[4]                | 24   | Slave #5 선택                   |
| CS[5]                | 23   | Slave #6 선택                   |
| CS[6]                | 0    | Slave #7 선택                   |
| CS[7]                | 27   | Slave #8 선택                   |
| CS[8]                | 4    | Slave #9 선택                   |
| CS[9]                | 5    | Slave #10 선택                  |
| CS[10]               | 28   | Slave #11 선택                  |
| CS[11]               | 12   | Slave #12 선택                  |

### Slave ESP32-C5 (각각)

| 기능     | GPIO | 용도                               |
| -------- | ---- | ---------------------------------- |
| SPI MOSI | 15   | Master 데이터 수신                 |
| SPI SCK  | 3    | 클럭 수신                          |
| SPI CS   | 7    | 칩 선택 (Master의 CS_PINS[x] 연결) |
| SPI SYNC | 16   | 동기화 신호 수신                   |
| LED OUT  | 8    | WS2812 데이터 출력 (1440 LEDs)     |

## 하드웨어 연결 가이드

### 공통 연결 (모든 Slave)

```
Master → Slave (모두)
GPIO 15 → GPIO 15 (MOSI)
GPIO 3  → GPIO 3  (SCK)
GPIO 16 → GPIO 16 (SYNC)
GND     → GND
```

### 개별 연결 (각 Slave의 CS)

```
Master CS → Slave CS
GPIO 1  → Slave #1 GPIO 7
GPIO 2  → Slave #2 GPIO 7
GPIO 25 → Slave #3 GPIO 7
GPIO 11 → Slave #4 GPIO 7
GPIO 24 → Slave #5 GPIO 7
GPIO 23 → Slave #6 GPIO 7
GPIO 0  → Slave #7 GPIO 7
GPIO 27 → Slave #8 GPIO 7
GPIO 4  → Slave #9 GPIO 7
GPIO 5  → Slave #10 GPIO 7
GPIO 28 → Slave #11 GPIO 7
GPIO 12 → Slave #12 GPIO 7
```

### LED 연결 (각 Slave)

```
Slave GPIO 8 → WS2812 Strip DIN (1440 LEDs)
```

## 주의사항

1. ⚠️ **모든 ESP32-C5의 GND를 공통 연결** (필수!)
2. ⚠️ **SPI 신호선은 최대한 짧게** (20cm 이내 권장)
3. ⚠️ **각 Slave는 독립된 5V 전원 필요** (1440 LEDs = 최대 86A)
4. ⚠️ **GPIO 1은 SD MISO와 CS[0] 공유** (SD 읽기 중에는 Slave #1 선택 안 됨)

## 소프트웨어 설정

### Master ESP32-C5

1. Arduino IDE에서 `master_spi/master_spi.ino` 열기
2. 보드: "ESP32C5 Dev Module" 선택
3. 업로드
4. SD 카드에 `1.bin`, `2.bin`, `3.bin`, `4.bin` 저장
5. `/sd/` 폴더에 파일 배치

### Slave ESP32-C5 (각각)

1. Arduino IDE에서 `slave_spi/slave_spi.ino` 열기
2. **`SLAVE_ID` 수정** (중요!)
   ```cpp
   const int SLAVE_ID = 1; // ← Slave #1은 1, Slave #2는 2, ... Slave #12는 12
   ```
3. 보드: "ESP32C5 Dev Module" 선택
4. 업로드
5. 반복 (12개 모두)

## 예상 성능

| 항목          | 시간                     |
| ------------- | ------------------------ |
| SD 읽기       | 36ms (50KB @ 1400 KB/s)  |
| SPI 전송      | 2ms (4320 bytes @ 20MHz) |
| LED 출력      | 43ms (1440 LEDs × 30μs)  |
| **총 프레임** | **~80ms**                |
| **예상 FPS**  | **12-15 FPS** ✓          |

**현재 단일 ESP32 성능: 2.1 FPS**
**Multi-ESP32 성능: 12-15 FPS (6-7배 개선!)** 🚀

## 장점 vs 단점

### ✅ 장점

- 12배 빠른 LED 출력 (병렬 처리)
- 동기화된 화면 (SYNC 신호)
- 확장 가능 (Slave 추가/제거)
- 단일 ESP32 대비 **6-7배 빠른 FPS**

### ❌ 단점

- 하드웨어 복잡도 증가 (ESP32-C5 13개 필요)
- 배선 복잡 (SPI + 12 CS 선)
- 비용 증가 (ESP32-C5 × 13)
- 전력 소모 증가

## 테스트 순서

1. **Slave #1 단독 테스트**

   - Slave 1개만 연결
   - `SLAVE_ID = 1` 설정
   - Master + Slave #1 전원 on
   - 시리얼 모니터로 통신 확인

2. **Slave 추가**

   - 정상 동작 확인 후 Slave #2 추가
   - `SLAVE_ID = 2` 설정
   - 반복

3. **전체 시스템 테스트**
   - 12개 모두 연결 후 전원 on
   - 버튼으로 파일 재생
   - FPS 확인

## 트러블슈팅

### 문제: Slave가 데이터를 받지 못함

- CS 연결 확인 (각 Slave마다 다른 Master CS 핀)
- GND 공통 연결 확인
- SPI 속도 낮추기 (20MHz → 10MHz)

### 문제: LED가 깜빡임

- 전원 부족 (5V 전원 용량 확인)
- SYNC 신호 확인
- FastLED.show() 타이밍 확인

### 문제: FPS가 낮음

- SD 카드 속도 확인 (Class 10 이상)
- SPI 속도 높이기 (20MHz → 40MHz)
- 시리얼 출력 줄이기

### 문제: Slave #1이 동작 안 함

- GPIO 1이 SD MISO와 공유됨
- SD 읽기 완료 후 CS[0] 선택되도록 타이밍 확인
- 또는 다른 GPIO로 CS[0] 변경
