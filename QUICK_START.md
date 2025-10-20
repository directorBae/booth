# 빠른 시작 가이드

## 1. 하드웨어 준비

### 필요한 부품

- ESP32-C5 × 11개 (Parent 1개 + Child 10개)
- Micro SD 카드 × 10개 (Class 10 이상)
- WS2812 LED Strip × 10개 (각 1440 LEDs = 180×8)
- 버튼 × 5개 (파일 1,2,3,4, 정지)
- 5V 전원 × 10개 (각 100A 권장)
- 점퍼 와이어 (SYNC, UART, GND 연결용)

## 2. 펌웨어 업로드

### Parent ESP32-C5 업로드

1. Arduino IDE 열기
2. 보드 선택: **"ESP32C5 Dev Module"**
3. `parent_controller/parent_controller.ino` 열기
4. 업로드 (Ctrl+U)
5. 시리얼 모니터 확인 (115200 baud)
   ```
   Parent ESP32-C5 Controller
   ✓ Buttons initialized
   ✓ SYNC pin initialized (GPIO 0)
   ✓ UART initialized (TX: GPIO 23, 115200 baud)
   Parent Ready!
   ```

### Child ESP32-C5 업로드 (10개)

**각 Child마다 반복:**

1. Arduino IDE 열기
2. 보드 선택: **"ESP32C5 Dev Module"**
3. `child_player/child_player.ino` 열기
4. **STRIP_ID 수정** (중요!):
   ```cpp
   const int STRIP_ID = 1;  // ← Child #1은 1, #2는 2, ..., #10은 10
   ```
5. 업로드 (Ctrl+U)
6. 시리얼 모니터 확인:
   ```
   Child ESP32-C5 #1
   ✓ SD Card initialized: 32768 MB
   ✓ FastLED initialized: 1440 LEDs (GPIO 5)
   ✓ SYNC interrupt attached (GPIO 0)
   ✓ UART initialized (RX: GPIO 23, 115200 baud)
   Child #1 Ready!
   ```

## 3. SD 카드 파일 준비

### 비디오 파일 분할

1. 180×80 비디오 .bin 파일 준비
2. Python 스크립트 실행:
   ```powershell
   python split_video_file.py video_180x80.bin
   ```
3. 출력 확인:
   ```
   strip_01.bin
   strip_02.bin
   strip_03.bin
   ...
   strip_10.bin
   ```

### SD 카드에 복사

**각 SD 카드에:**

1. FAT32로 포맷
2. 파일 이름 변경:
   - `strip_01.bin` → `file1.bin` (버튼 1용)
   - `strip_01.bin` → `file2.bin` (버튼 2용)
   - `strip_01.bin` → `file3.bin` (버튼 3용)
   - `strip_01.bin` → `file4.bin` (버튼 4용)
3. SD 카드 루트에 복사

**결과:**

```
SD Card #1 (Child #1):
  - file1.bin
  - file2.bin
  - file3.bin
  - file4.bin

SD Card #2 (Child #2):
  - file1.bin
  - file2.bin
  - file3.bin
  - file4.bin

... (동일)

SD Card #10 (Child #10):
  - file1.bin
  - file2.bin
  - file3.bin
  - file4.bin
```

## 4. 하드웨어 연결

### Parent ESP32-C5 연결

```
버튼 연결:
  Button 1 (파일 1)  → GPIO 9  + GND
  Button 2 (파일 2)  → GPIO 10 + GND
  Button 3 (파일 3)  → GPIO 13 + GND
  Button 4 (파일 4)  → GPIO 14 + GND
  Button Stop (정지) → GPIO 26 + GND

신호 출력:
  SYNC 신호  → GPIO 0  → 모든 Child GPIO 0
  UART TX    → GPIO 23 → 모든 Child GPIO 23
  GND        → 모든 Child GND (공통)
```

### Child ESP32-C5 연결 (각각)

```
SD 카드:
  MISO → GPIO 1
  MOSI → GPIO 6
  SCK  → GPIO 7
  CS   → GPIO 8

LED Strip:
  DIN  → GPIO 5

신호 입력:
  SYNC → GPIO 0  (Parent GPIO 0에서)
  RX   → GPIO 23 (Parent GPIO 23에서)
  GND  → Parent GND
```

### 연결 다이어그램

```
Parent ESP32-C5
   ├─ GPIO 0  ─┬─ Child #1 GPIO 0
   │           ├─ Child #2 GPIO 0
   │           ├─ Child #3 GPIO 0
   │           └─ ... Child #10 GPIO 0 (SYNC)
   │
   ├─ GPIO 23 ─┬─ Child #1 GPIO 23
   │           ├─ Child #2 GPIO 23
   │           ├─ Child #3 GPIO 23
   │           └─ ... Child #10 GPIO 23 (UART)
   │
   └─ GND ─────┴─ 모든 Child GND (공통)
```

## 5. 테스트

### 1단계: Parent 단독 테스트

1. Parent 전원 on
2. 시리얼 모니터 확인
3. 버튼 1 누르기
4. 출력 확인:
   ```
   [START] Playing file 1
   Command sent to all Children via UART
   SYNC signals will be sent every 44ms (22 FPS)
   [SYNC] File 1 playing... (22 FPS)
   ```

### 2단계: Child #1 추가

1. SD Card #1 삽입
2. LED Strip #1 연결 (GPIO 5)
3. 공통 신호 연결 (SYNC, UART, GND)
4. Child #1 전원 on
5. 시리얼 모니터 확인:
   ```
   Child #1 Ready!
   Waiting for command from Parent...
   ```
6. Parent 버튼 1 누르기
7. Child 출력 확인:
   ```
   [OPEN] Opening /file1.bin...
   Resolution: 180x8
   FPS: 22
   Frames: 300
   ✓ Child #1 ready to play file 1
   [FRAME] Child #1: 0/300 (0.0%)
   [FRAME] Child #1: 30/300 (10.0%)
   ```
8. LED Strip 재생 확인

### 3단계: 나머지 Child 추가

각 Child마다:

1. STRIP_ID 설정
2. 업로드
3. SD Card 삽입
4. LED Strip 연결
5. 공통 신호 연결
6. 전원 on
7. 테스트

### 4단계: 전체 시스템 테스트

1. 모든 ESP32 (1+10=11개) 전원 on
2. Parent 버튼 1 누르기
3. 확인:
   - 모든 Child가 명령 수신
   - 모든 LED Strip (1~10) 동시 재생
   - 프레임 동기화 (SYNC 신호)
4. 버튼 2, 3, 4 테스트
5. Stop 버튼 테스트

## 6. 문제 해결

### Parent가 명령을 전송하지 않음

- 버튼 연결 확인 (풀업 저항 또는 INPUT_PULLUP)
- 시리얼 모니터에서 버튼 입력 확인

### Child가 명령을 받지 못함

- **GND 공통 연결 확인** (가장 흔한 실수!)
- UART 연결: Parent GPIO 23 → Child GPIO 23
- 시리얼 모니터에서 수신 확인

### Child가 SD 카드를 읽지 못함

- SD 카드 포맷 (FAT32)
- 파일 이름 확인 (`file1.bin`, `file2.bin`, ...)
- SD 핀 연결 확인 (1,6,7,8)

### LED가 안 켜짐

- LED 전원 확인 (5V, 100A)
- LED DIN 연결 확인 (GPIO 5)
- FastLED 밝기 확인 (코드에서 50%)

### LED가 동기화 안 됨

- SYNC 연결 확인: Parent GPIO 0 → 모든 Child GPIO 0
- GND 공통 연결 확인
- 시리얼 모니터에서 SYNC 수신 확인

### FPS가 낮음

- SD 카드 속도 (Class 10 이상)
- 시리얼 출력 줄이기 (릴리즈 시 주석 처리)
- LED 밝기 낮추기 (전력 절약)

## 7. 성능 최적화

### 릴리즈 빌드

코드에서 디버깅 출력 제거:

```cpp
// Child 코드에서
// Serial.printf(...) 주석 처리

// 또는 컴파일 옵션 사용
#define DEBUG 0

#if DEBUG
    Serial.printf(...);
#endif
```

### SD 카드 속도

- Class 10 → UHS-I (U1 또는 U3)
- 읽기 속도: 20MB/s → 40MB/s

### UART 속도

이미 115200 baud 사용 중 (최적)

### LED 밝기

```cpp
FastLED.setBrightness(50);  // 50% (기본)
FastLED.setBrightness(30);  // 30% (전력 절약)
FastLED.setBrightness(100); // 100% (최대 밝기, 전력 주의)
```

## 8. 파일 구조

```
NEOPIXEL/
├── parent_controller/
│   └── parent_controller.ino    # Parent 코드
├── child_player/
│   └── child_player.ino         # Child 코드
├── split_video_file.py          # 파일 분할 스크립트
├── README.md                    # 메인 문서
├── PIN_CONFIG_SUMMARY.md        # 핀 구성 요약
└── QUICK_START.md               # 이 문서
```

## 9. 다음 단계

- [ ] 모든 ESP32 업로드 완료
- [ ] SD 카드 파일 준비 완료
- [ ] 하드웨어 연결 완료
- [ ] Parent 단독 테스트 통과
- [ ] Child #1 테스트 통과
- [ ] 전체 시스템 테스트 통과
- [ ] 성능 측정 (FPS 확인)
- [ ] 릴리즈 빌드 (디버깅 출력 제거)

## 10. 추가 리소스

- **README.md**: 전체 시스템 상세 문서
- **PIN_CONFIG_SUMMARY.md**: 핀 구성 빠른 참조
- **GitHub Issues**: 문제 발생 시 이슈 등록

---

**성공을 기원합니다! 🚀**
