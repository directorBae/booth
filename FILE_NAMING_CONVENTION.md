# 파일 명명 규칙 (File Naming Convention)

## 개요

각 Child ESP32-C5의 SD 카드에는 4개의 미디어 파일이 저장되며, 파일명은 다음과 같은 형식을 따릅니다:

```
{미디어번호}_{Strip ID}.bin
```

## 파일명 구성

### 첫 번째 숫자: 미디어 번호 (1-4)

- **1**: 버튼 1 (GPIO 9)로 재생되는 미디어
- **2**: 버튼 2 (GPIO 10)로 재생되는 미디어
- **3**: 버튼 3 (GPIO 13)로 재생되는 미디어
- **4**: 버튼 4 (GPIO 14)로 재생되는 미디어

### 두 번째 숫자: Strip ID (1-10)

- **1-10**: Child ESP32-C5의 고유 번호
- 각 Child는 자신의 Strip ID에 해당하는 파일만 읽습니다

## 파일명 예시

### Child #1의 SD 카드

```
1_1.bin  (미디어 1, Strip 1)
2_1.bin  (미디어 2, Strip 1)
3_1.bin  (미디어 3, Strip 1)
4_1.bin  (미디어 4, Strip 1)
```

### Child #3의 SD 카드

```
1_3.bin  (미디어 1, Strip 3)
2_3.bin  (미디어 2, Strip 3)
3_3.bin  (미디어 3, Strip 3)
4_3.bin  (미디어 4, Strip 3)
```

### Child #10의 SD 카드

```
1_10.bin  (미디어 1, Strip 10)
2_10.bin  (미디어 2, Strip 10)
3_10.bin  (미디어 3, Strip 10)
4_10.bin  (미디어 4, Strip 10)
```

## 전체 시스템 파일 목록

총 40개의 파일 (4개 미디어 × 10개 Strip):

```
1_1.bin   1_2.bin   1_3.bin   1_4.bin   1_5.bin   1_6.bin   1_7.bin   1_8.bin   1_9.bin   1_10.bin
2_1.bin   2_2.bin   2_3.bin   2_4.bin   2_5.bin   2_6.bin   2_7.bin   2_8.bin   2_9.bin   2_10.bin
3_1.bin   3_2.bin   3_3.bin   3_4.bin   3_5.bin   3_6.bin   3_7.bin   3_8.bin   3_9.bin   3_10.bin
4_1.bin   4_2.bin   4_3.bin   4_4.bin   4_5.bin   4_6.bin   4_7.bin   4_8.bin   4_9.bin   4_10.bin
```

## 동작 방식

1. **Parent 버튼 입력**

   - 사용자가 버튼 1-4 중 하나를 누름

2. **UART 명령 전송**

   - Parent가 모든 Child에게 미디어 번호 (1-4)를 UART로 브로드캐스트

3. **파일 선택**

   - 각 Child는 수신한 미디어 번호와 자신의 Strip ID를 조합하여 파일명 생성
   - 예: Child #3이 미디어 2 명령을 받으면 → `2_3.bin` 파일 열기

4. **동기화 재생**
   - 모든 Child가 자신의 파일을 읽고 SYNC 신호에 맞춰 동시에 LED 출력

## 파일 준비 과정

### 1단계: 원본 영상 분할

`split_video_file.py` 스크립트를 사용하여 180×80 영상을 10개의 180×8 파일로 분할:

```bash
python split_video_file.py media1_original.bin
# 출력: 1_1.bin, 1_2.bin, ..., 1_10.bin
```

### 2단계: SD 카드 복사

각 Child의 SD 카드에 해당하는 파일 복사:

- **Child #1 SD 카드**: `1_1.bin`, `2_1.bin`, `3_1.bin`, `4_1.bin`
- **Child #2 SD 카드**: `1_2.bin`, `2_2.bin`, `3_2.bin`, `4_2.bin`
- **Child #3 SD 카드**: `1_3.bin`, `2_3.bin`, `3_3.bin`, `4_3.bin`
- ... (계속)
- **Child #10 SD 카드**: `1_10.bin`, `2_10.bin`, `3_10.bin`, `4_10.bin`

## 코드에서의 파일명 생성

### child_player.ino

```cpp
const int STRIP_ID = 3; // Child #3

void startPlayback(uint8_t fileNum)
{
    // fileNum: 1-4 (Parent에서 UART로 수신)
    // STRIP_ID: 1-10 (컴파일 시 설정)

    char filename[20];
    sprintf(filename, "/%d_%d.bin", fileNum, STRIP_ID);
    // 결과: "/1_3.bin", "/2_3.bin", "/3_3.bin", "/4_3.bin"

    sdFile = SD.open(filename, FILE_READ);
    // ...
}
```

## 장점

1. **명확성**: 파일명만 보고도 미디어 종류와 Strip 번호를 즉시 파악 가능
2. **독립성**: 각 Child가 자신의 파일만 관리하므로 SD 카드 교체/디버깅 용이
3. **확장성**: 미디어 추가 시 파일명 패턴만 유지하면 됨
4. **디버깅**: 로그에서 어느 Child가 어떤 파일을 재생 중인지 명확히 표시

## 문제 해결

### 파일을 찾을 수 없음

```
✗ Failed to open /2_5.bin
Please ensure file 2_5.bin exists on SD card
```

**해결 방법**:

1. SD 카드에 `2_5.bin` 파일이 있는지 확인
2. 파일명이 정확한지 확인 (대소문자, 언더바 위치)
3. 파일이 SD 카드 루트 디렉토리에 있는지 확인

### Strip ID 불일치

만약 Child #7인데 `1_3.bin` 파일을 찾으려 한다면:

- `child_player.ino`의 `STRIP_ID` 값이 잘못 설정됨
- 각 Child에 업로드할 때 `STRIP_ID`를 올바르게 수정해야 함

## 체크리스트

SD 카드 준비 시 확인 사항:

- [ ] 4개의 미디어 파일이 모두 존재 (`X_Y.bin` 형식)
- [ ] 파일명의 Strip ID가 Child 번호와 일치
- [ ] 파일이 SD 카드 루트에 위치
- [ ] 파일 크기가 0보다 큼
- [ ] SD 카드가 FAT32로 포맷됨
