#include <SPI.h>
#include <SD.h>
#include <FastLED.h>

// === SD 카드 핀 설정 (최종 확인) ===
const int PIN_MISO = 1;
const int PIN_MOSI = 6;
const int PIN_SCK = 7;
const int PIN_CS = 8;
const uint32_t SD_SPI_HZ = 20000000; // SD카드 속도 (20MHz)

// === 버튼 핀 설정 (최종 확인) ===
const int BUTTON_FILE_1 = 10; // 1.bin 재생 버튼
const int BUTTON_FILE_2 = 9;  // 2.bin 재생 버튼
const int BUTTON_FILE_3 = 13; // 3.bin 재생 버튼
const int BUTTON_FILE_4 = 14; // 4.bin 재생 버튼
const int BUTTON_STOP = 26;   // 정지 버튼

// === LED 설정 (최종 확인) ===
const int NUM_STRIPS = 12;         // 12개의 LED 스트립
const int LEDS_PER_STRIP = 1440;   // 스트립 당 1440개 LED
const int LEDS_PER_ROW = 180;      // 한 줄에 180개 LED
const int NUM_ROWS = LEDS_PER_STRIP / LEDS_PER_ROW; // 스트립 당 8줄

// === 재생 설정 ===
const unsigned long FRAME_INTERVAL = 1000 / 23; // 23 FPS (약 43ms)

// === 파일 헤더 구조체 ===
struct BinFileHeader
{
  uint32_t width;
  uint32_t height;
  float fps;
  uint32_t frameCount;
};

// === 전역 변수 ===
CRGB leds[NUM_STRIPS][LEDS_PER_STRIP];
BinFileHeader fileHeader;
File binFile;

// 재생 상태
bool isPlaying = false;
uint32_t currentFrame = 0;
unsigned long lastFrameTime = 0;
uint32_t headerSize = 0;
String currentFileName = "";

// 버튼 디바운싱
unsigned long lastButtonTime = 0;
const unsigned long DEBOUNCE_DELAY = 200; // 200ms

void setup()
{
  Serial.begin(115200);
  delay(300);
  Serial.println("\n==== Multi-Strip LED Matrix Player (Final Wiring) ====");

  // 버튼 핀 초기화
  pinMode(BUTTON_FILE_1, INPUT_PULLUP);
  pinMode(BUTTON_FILE_2, INPUT_PULLUP);
  pinMode(BUTTON_FILE_3, INPUT_PULLUP);
  pinMode(BUTTON_FILE_4, INPUT_PULLUP);
  pinMode(BUTTON_STOP, INPUT_PULLUP);
  Serial.println("Buttons initialized.");

  // LED 초기화 (컴파일 오류 수정)
  // FastLED.addLeds는 핀 번호가 컴파일 타임 상수로 지정되어야 합니다.
  FastLED.addLeds<WS2812, 3, GRB>(leds[0], LEDS_PER_STRIP);  // Strip 1
  FastLED.addLeds<WS2812, 2, GRB>(leds[1], LEDS_PER_STRIP);  // Strip 2
  FastLED.addLeds<WS2812, 25, GRB>(leds[2], LEDS_PER_STRIP); // Strip 3
  FastLED.addLeds<WS2812, 11, GRB>(leds[3], LEDS_PER_STRIP); // Strip 4
  FastLED.addLeds<WS2812, 24, GRB>(leds[4], LEDS_PER_STRIP); // Strip 5
  FastLED.addLeds<WS2812, 23, GRB>(leds[5], LEDS_PER_STRIP); // Strip 6
  FastLED.addLeds<WS2812, 15, GRB>(leds[6], LEDS_PER_STRIP); // Strip 7
  FastLED.addLeds<WS2812, 27, GRB>(leds[7], LEDS_PER_STRIP); // Strip 8
  FastLED.addLeds<WS2812, 4, GRB>(leds[8], LEDS_PER_STRIP);  // Strip 9
  FastLED.addLeds<WS2812, 5, GRB>(leds[9], LEDS_PER_STRIP);  // Strip 10
  FastLED.addLeds<WS2812, 28, GRB>(leds[10], LEDS_PER_STRIP);// Strip 11
  FastLED.addLeds<WS2812, 12, GRB>(leds[11], LEDS_PER_STRIP);// Strip 12
  
  FastLED.setBrightness(100);
  FastLED.clear();
  FastLED.show();
  Serial.printf("LEDs initialized: %d strips, %d pixels per strip.\n", NUM_STRIPS, LEDS_PER_STRIP);

  // SD 카드 초기화
  initSDCard();

  Serial.println("[READY] Press a button to play a file.");
}

void loop()
{
  checkButtons();

  if (isPlaying)
  {
    unsigned long now = millis();
    if (now - lastFrameTime >= FRAME_INTERVAL)
    {
      playNextFrame();
      lastFrameTime = now;
    }
  }
  else
  {
    delay(10);
  }
}

void checkButtons()
{
  unsigned long now = millis();
  if (now - lastButtonTime < DEBOUNCE_DELAY) return;

  // 1.bin 재생 버튼
  if (digitalRead(BUTTON_FILE_1) == LOW) {
    lastButtonTime = now;
    if (!isPlaying || currentFileName != "1.bin") {
      startPlayback("/sd/1.bin");
    }
  }

  // 2.bin 재생 버튼
  if (digitalRead(BUTTON_FILE_2) == LOW) {
    lastButtonTime = now;
    if (!isPlaying || currentFileName != "2.bin") {
      startPlayback("/sd/2.bin");
    }
  }
  
  // 3.bin 재생 버튼
  if (digitalRead(BUTTON_FILE_3) == LOW) {
    lastButtonTime = now;
    if (!isPlaying || currentFileName != "3.bin") {
      startPlayback("/sd/3.bin");
    }
  }

  // 4.bin 재생 버튼
  if (digitalRead(BUTTON_FILE_4) == LOW) {
    lastButtonTime = now;
    if (!isPlaying || currentFileName != "4.bin") {
      startPlayback("/sd/4.bin");
    }
  }

  // 정지 버튼
  if (digitalRead(BUTTON_STOP) == LOW) {
    lastButtonTime = now;
    if (isPlaying) {
      stopPlayback();
    }
  }
}

void initSDCard()
{
  Serial.printf("[SD Pins] MISO=%d, MOSI=%d, SCK=%d, CS=%d\n", PIN_MISO, PIN_MOSI, PIN_SCK, PIN_CS);
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);

  if (!SD.begin(PIN_CS, SPI, SD_SPI_HZ)) {
    Serial.println("[FAIL] SD.begin() failed! Check wiring or card.");
    return;
  }
  Serial.printf("[OK] SD mounted. Type=%s, Size=%llu MB\n", SD.cardType(), SD.cardSize() / (1024ULL * 1024ULL));
}

void startPlayback(const char *filePath)
{
  if (isPlaying) stopPlayback();
  
  Serial.printf("\n[PLAY] Button pressed. Starting playback of %s...\n", filePath);

  binFile = SD.open(filePath, FILE_READ);
  if (!binFile) {
    Serial.printf("[ERROR] Cannot open %s\n", filePath);
    return;
  }

  currentFileName = String(filePath).substring(String(filePath).lastIndexOf('/') + 1);

  headerSize = sizeof(BinFileHeader);
  if (binFile.read((uint8_t *)&fileHeader, headerSize) != headerSize) {
    Serial.println("[ERROR] Failed to read header");
    binFile.close();
    return;
  }

  Serial.printf("[INFO] Width=%u, Height=%u, FPS=%.1f, Frames=%u\n",
                fileHeader.width, fileHeader.height, fileHeader.fps, fileHeader.frameCount);

  isPlaying = true;
  currentFrame = 0;
  lastFrameTime = millis();
}

void stopPlayback()
{
  Serial.println("\n[STOP] Button pressed. Stopping playback...");
  isPlaying = false;
  currentFileName = "";
  if (binFile) binFile.close();
  
  FastLED.clear();
  FastLED.show();
  Serial.println("[STOPPED] All LEDs OFF");
}

void playNextFrame()
{
  if (!binFile || !isPlaying) return;

  if (currentFrame >= fileHeader.frameCount) {
    Serial.println("\n[LOOP] Restarting playback...");
    currentFrame = 0;
    if (!binFile.seek(headerSize)) {
        Serial.println("[ERROR] Failed to seek back to start.");
        stopPlayback();
        return;
    }
  }

  for (int s = 0; s < NUM_STRIPS; s++) {
    static uint8_t stripData[LEDS_PER_STRIP * 3];
    int bytesRead = binFile.read(stripData, sizeof(stripData));
    if (bytesRead != sizeof(stripData)) {
      Serial.printf("[ERROR] Failed to read data for strip %d at frame %u.\n", s, currentFrame);
      stopPlayback();
      return;
    }

    for (int row = 0; row < NUM_ROWS; row++) {
      for (int col = 0; col < LEDS_PER_ROW; col++) {
        int sourceDataIndex = (row * LEDS_PER_ROW + col) * 3;
        int targetLedIndex;
        if ((row % 2) == 1) {
          targetLedIndex = (row * LEDS_PER_ROW) + (LEDS_PER_ROW - 1 - col);
        } else {
          targetLedIndex = (row * LEDS_PER_ROW) + col;
        }

        leds[s][targetLedIndex] = CRGB(
          stripData[sourceDataIndex + 2], // Red
          stripData[sourceDataIndex + 1], // Green
          stripData[sourceDataIndex]      // Blue
        );
      }
    }
  }

  FastLED.show();

  if (currentFrame % 30 == 0) {
    float progress = (float)currentFrame / fileHeader.frameCount * 100.0;
    Serial.printf("[FRAME] %u/%u (%.1f%%)\n", currentFrame, fileHeader.frameCount, progress);
  }

  currentFrame++;
}

