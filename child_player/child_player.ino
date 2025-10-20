// ============================================
// Child ESP32-C5 Player
// ============================================
// 역할: SD 카드에서 프레임 읽기 + LED Strip 제어
// UART로 재생 명령 수신, SYNC 신호로 동기화
//
// 파일명 형식: {미디어번호}_{Strip ID}.bin
// 예시: 1_1.bin, 1_2.bin, ..., 2_5.bin, ..., 4_10.bin
// - 첫 번째 숫자(1-4): 미디어 종류 (버튼 1-4)
// - 두 번째 숫자(1-10): Strip ID (Child 번호)
//
// SD 카드 파일 구성 예시 (Child #3):
//   - 1_3.bin (미디어 1용)
//   - 2_3.bin (미디어 2용)
//   - 3_3.bin (미디어 3용)
//   - 4_3.bin (미디어 4용)

#include <FastLED.h>
#include <SPI.h>
#include <SD.h>

// ============================================
// ※ 중요: 각 Child마다 STRIP_ID를 다르게 설정!
// ============================================
const int STRIP_ID = 1; // Child #1은 1, #2는 2, ..., #10은 10

// === SD 카드 핀 설정 ===
const int PIN_MISO = 1;
const int PIN_MOSI = 6;
const int PIN_SCK = 7;
const int PIN_CS = 8;

// === LED 핀 설정 ===
const int PIN_LED = 5; // WS2812 데이터 핀

// === 동기화 핀 설정 ===
const int PIN_SYNC = 0; // Parent GPIO 0에서 수신

// === UART 핀 설정 ===
const int PIN_RX = 23; // Parent GPIO 23에서 수신

// === LED 설정 ===
const int LEDS_PER_STRIP = 1440; // 180×8
const int LEDS_PER_ROW = 180;
const int NUM_ROWS = 8;

// === 파일 헤더 구조체 ===
struct BinFileHeader
{
    uint32_t width;
    uint32_t height;
    uint32_t fps;
    uint32_t frameCount;
};

// === 전역 변수 ===
CRGB leds[LEDS_PER_STRIP];
uint8_t frameBuffer[LEDS_PER_STRIP * 3]; // BGR 데이터
File sdFile;
BinFileHeader fileHeader;

volatile bool syncReceived = false;
bool frameReady = false;
bool isPlaying = false;
uint8_t currentFileNum = 0;
uint32_t currentFrame = 0;

const size_t HEADER_SIZE = 16;
const size_t STRIP_BYTES = LEDS_PER_STRIP * 3; // 4320 bytes

HardwareSerial SerialUART(1); // Serial1 사용

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=========================================");
    Serial.printf("  Child ESP32-C5 #%d\n", STRIP_ID);
    Serial.println("  - SD Card Reader");
    Serial.println("  - LED Strip Controller (1440 LEDs)");
    Serial.println("  - UART Command Receiver");
    Serial.println("  - SYNC Signal Receiver");
    Serial.println("=========================================");

    // === SD 카드 초기화 ===
    Serial.println("\n[1/4] Initializing SD Card...");
    SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);

    if (!SD.begin(PIN_CS))
    {
        Serial.println("  ✗ SD Card initialization FAILED!");
        Serial.println("  Please check:");
        Serial.println("    - SD card inserted");
        Serial.println("    - Connections: MISO=1, MOSI=6, SCK=7, CS=8");
        while (1)
            delay(1000);
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("  ✓ SD Card initialized: %llu MB\n", cardSize);

    // === FastLED 초기화 ===
    Serial.println("\n[2/4] Initializing FastLED...");
    FastLED.addLeds<WS2812, PIN_LED, GRB>(leds, LEDS_PER_STRIP);
    FastLED.setBrightness(50); // 50% 밝기 (전력 절약)
    FastLED.clear();
    FastLED.show();
    Serial.printf("  ✓ FastLED initialized: %d LEDs (GPIO %d)\n", LEDS_PER_STRIP, PIN_LED);

    // === SYNC 인터럽트 초기화 ===
    Serial.println("\n[3/4] Initializing SYNC interrupt...");
    pinMode(PIN_SYNC, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_SYNC), onSyncInterrupt, RISING);
    Serial.printf("  ✓ SYNC interrupt attached (GPIO %d)\n", PIN_SYNC);

    // === UART 초기화 ===
    Serial.println("\n[4/4] Initializing UART...");
    // RX: GPIO 23, TX: 사용 안 함
    SerialUART.begin(115200, SERIAL_8N1, PIN_RX, -1);
    Serial.printf("  ✓ UART initialized (RX: GPIO %d, 115200 baud)\n", PIN_RX);

    Serial.println("\n=========================================");
    Serial.printf("  Child #%d Ready!\n", STRIP_ID);
    Serial.println("  Waiting for command from Parent...");
    Serial.println("=========================================\n");
}

void loop()
{
    // UART 명령 수신
    if (SerialUART.available())
    {
        uint8_t command = SerialUART.read();
        handleCommand(command);
    }

    // SD 카드에서 다음 프레임 읽기
    if (frameReady && isPlaying)
    {
        readNextFrame();
        frameReady = false;
    }

    // SYNC 신호 수신 시 LED 출력
    if (syncReceived)
    {
        FastLED.show();
        syncReceived = false;
        frameReady = true; // 다음 프레임 준비
    }
}

void IRAM_ATTR onSyncInterrupt()
{
    syncReceived = true;
}

void handleCommand(uint8_t command)
{
    if (command >= 1 && command <= 4)
    {
        // 파일 1~4 재생
        startPlayback(command);
    }
    else if (command == 0)
    {
        // 정지
        stopPlayback();
    }
}

void startPlayback(uint8_t fileNum)
{
    if (isPlaying && currentFileNum == fileNum)
    {
        // 이미 같은 파일 재생 중
        return;
    }

    // 현재 파일 닫기
    if (sdFile)
    {
        sdFile.close();
    }

    // 파일 열기 (형식: fileNum_STRIP_ID.bin, 예: 1_3.bin, 2_5.bin 등)
    char filename[20];
    sprintf(filename, "/%d_%d.bin", fileNum, STRIP_ID);

    Serial.printf("\n[OPEN] Opening %s...\n", filename);

    sdFile = SD.open(filename, FILE_READ);
    if (!sdFile)
    {
        Serial.printf("  ✗ Failed to open %s\n", filename);
        Serial.printf("  Please ensure file %d_%d.bin exists on SD card\n", fileNum, STRIP_ID);
        isPlaying = false;
        return;
    }

    // 헤더 읽기
    uint8_t headerBuffer[HEADER_SIZE];
    if (sdFile.read(headerBuffer, HEADER_SIZE) != HEADER_SIZE)
    {
        Serial.println("  ✗ Failed to read file header");
        sdFile.close();
        isPlaying = false;
        return;
    }

    memcpy(&fileHeader.width, headerBuffer, 4);
    memcpy(&fileHeader.height, headerBuffer + 4, 4);
    memcpy(&fileHeader.fps, headerBuffer + 8, 4);
    memcpy(&fileHeader.frameCount, headerBuffer + 12, 4);

    Serial.printf("  Resolution: %dx%d\n", fileHeader.width, fileHeader.height);
    Serial.printf("  FPS: %d\n", fileHeader.fps);
    Serial.printf("  Frames: %d\n", fileHeader.frameCount);

    currentFileNum = fileNum;
    currentFrame = 0;
    isPlaying = true;
    frameReady = true;

    Serial.printf("  ✓ Child #%d ready to play file %d\n", STRIP_ID, fileNum);
}

void stopPlayback()
{
    if (sdFile)
    {
        sdFile.close();
    }

    isPlaying = false;
    currentFileNum = 0;
    currentFrame = 0;

    // LED 끄기
    FastLED.clear();
    FastLED.show();

    Serial.printf("\n[STOP] Child #%d playback stopped\n", STRIP_ID);
}

void readNextFrame()
{
    if (!sdFile || !isPlaying)
        return;

    // 루프 재생
    if (currentFrame >= fileHeader.frameCount)
    {
        Serial.printf("\n[LOOP] Child #%d restarting playback...\n", STRIP_ID);
        currentFrame = 0;
        if (!sdFile.seek(HEADER_SIZE))
        {
            Serial.println("  ✗ Failed to seek to start");
            stopPlayback();
            return;
        }
    }

    // SD 카드에서 프레임 데이터 읽기 (4320 bytes)
    int bytesRead = sdFile.read(frameBuffer, STRIP_BYTES);
    if (bytesRead != STRIP_BYTES)
    {
        Serial.printf("  ✗ Failed to read frame data (read %d bytes)\n", bytesRead);
        stopPlayback();
        return;
    }

    // BGR 데이터를 LED 배열로 변환 (지그재그 패턴)
    for (int row = 0; row < NUM_ROWS; row++)
    {
        for (int col = 0; col < LEDS_PER_ROW; col++)
        {
            int sourceIndex = (row * LEDS_PER_ROW + col) * 3;
            int targetIndex;

            // 지그재그 패턴 (홀수 행은 역방향)
            if (row % 2 == 1)
            {
                targetIndex = row * LEDS_PER_ROW + (LEDS_PER_ROW - 1 - col);
            }
            else
            {
                targetIndex = row * LEDS_PER_ROW + col;
            }

            uint8_t b = frameBuffer[sourceIndex];
            uint8_t g = frameBuffer[sourceIndex + 1];
            uint8_t r = frameBuffer[sourceIndex + 2];

            leds[targetIndex] = CRGB(r, g, b);
        }
    }

    // 진행률 출력 (매 30 프레임마다)
    if (currentFrame % 30 == 0)
    {
        float progress = (float)currentFrame / fileHeader.frameCount * 100.0;
        Serial.printf("[FRAME] Child #%d: %u/%u (%.1f%%)\n",
                      STRIP_ID, currentFrame, fileHeader.frameCount, progress);
    }

    currentFrame++;
}
