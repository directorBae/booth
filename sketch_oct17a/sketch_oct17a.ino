#include <SPI.h>
#include <SD.h>
#include <FastLED.h>

// === SD Card Pin Settings (Final) ===
const int PIN_MISO = 1;
const int PIN_MOSI = 6;
const int PIN_SCK = 7;
const int PIN_CS = 8;
const uint32_t SD_SPI_HZ = 40000000; // SD Card Speed (40MHz - 속도 향상)

// === Button Pin Settings (Final) ===
const int BUTTON_FILE_1 = 10;
const int BUTTON_FILE_2 = 9;
const int BUTTON_FILE_3 = 13;
const int BUTTON_FILE_4 = 14;
const int BUTTON_STOP = 26;

// === LED Settings ===
const int LEDS_PER_ROW = 180;
const int NUM_ROWS = 100;
const int TOTAL_LEDS = LEDS_PER_ROW * NUM_ROWS; // 18000개

// 2개의 체인 (ESP32-C5의 RMT TX 채널 제한)
const int NUM_PINS = 2;
const int LEDS_PER_PIN = 8640; // 각 핀당 8640개 (6 strips × 1440 LEDs)
const int LED_PINS[NUM_PINS] = {3, 2};

const int STRIPS_PER_PIN = 6; // 각 핀당 6개 스트립
const int LEDS_PER_STRIP = 1440;
const int NUM_ROWS_PER_STRIP = LEDS_PER_STRIP / LEDS_PER_ROW; // 8 rows

// === Playback Settings ===
const unsigned long FRAME_INTERVAL = 1; // Play as fast as hardware allows

// === File Header Struct ===
struct BinFileHeader
{
  uint32_t width;
  uint32_t height;
  float fps;
  uint32_t frameCount;
};

// === Global Variables ===
CRGB leds[NUM_PINS][LEDS_PER_PIN]; // FastLED 배열
BinFileHeader fileHeader;
File binFile;

// Playback state
bool isPlaying = false;
uint32_t currentFrame = 0;
unsigned long lastFrameTime = 0;
uint32_t headerSize = 0;
String currentFileName = "";

// Button debouncing
unsigned long lastButtonTime = 0;
const unsigned long DEBOUNCE_DELAY = 200;

void setup()
{
  Serial.begin(115200);
  delay(1000); // 시리얼 모니터가 열릴 시간 확보

  Serial.println("\n\n\n"); // 줄바꿈으로 명확하게
  Serial.println("=====================================");
  Serial.println("  Multi-Strip LED Player");
  Serial.println("  ESP32-C5 Initialization");
  Serial.println("=====================================");

  // STEP 1: Initialize buttons first (no conflicts)
  Serial.println("\n[STEP 1/3] Button Initialization");
  pinMode(BUTTON_FILE_1, INPUT_PULLUP);
  pinMode(BUTTON_FILE_2, INPUT_PULLUP);
  pinMode(BUTTON_FILE_3, INPUT_PULLUP);
  pinMode(BUTTON_FILE_4, INPUT_PULLUP);
  pinMode(BUTTON_STOP, INPUT_PULLUP);
  Serial.println("  ✓ All buttons configured");

  // STEP 2: Initialize SD Card BEFORE LED to prevent SPI conflicts
  Serial.println("\n[STEP 2/3] SD Card Initialization");
  initSDCard();
  delay(100); // SD 초기화 후 대기

  // STEP 3: FastLED 초기화 (2개 체인, RMT TX 채널 제한)
  Serial.println("\n[STEP 3/3] FastLED Initialization (2 chains)");

  FastLED.addLeds<WS2812, 3, GRB>(leds[0], LEDS_PER_PIN); // Chain 0: Strips 1-6 (8640 LEDs)
  FastLED.addLeds<WS2812, 2, GRB>(leds[1], LEDS_PER_PIN); // Chain 1: Strips 7-12 (8640 LEDs)

  Serial.printf("  ✓ Both chains initialized (%d LEDs each = 6 strips)\n", LEDS_PER_PIN);

  FastLED.setBrightness(100);
  FastLED.clear();
  FastLED.show();

  Serial.println("  ✓ FastLED initialized");

  Serial.println("\n=====================================");
  Serial.printf("  System Ready!\n");
  Serial.printf("  Total LEDs: %d (%d chains × %d LEDs)\n",
                NUM_PINS * LEDS_PER_PIN, NUM_PINS, LEDS_PER_PIN);
  Serial.println("=====================================");
  Serial.println("\nPress buttons to play files:");
  Serial.println("  GPIO 10 → 1.bin");
  Serial.println("  GPIO  9 → 2.bin");
  Serial.println("  GPIO 13 → 3.bin");
  Serial.println("  GPIO 14 → 4.bin");
  Serial.println("  GPIO 26 → STOP\n");
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
  if (now - lastButtonTime < DEBOUNCE_DELAY)
    return;

  if (digitalRead(BUTTON_FILE_1) == LOW)
  {
    lastButtonTime = now;
    if (!isPlaying || currentFileName != "1.bin")
      startPlayback("/sd/1.bin");
  }
  else if (digitalRead(BUTTON_FILE_2) == LOW)
  {
    lastButtonTime = now;
    if (!isPlaying || currentFileName != "2.bin")
      startPlayback("/sd/2.bin");
  }
  else if (digitalRead(BUTTON_FILE_3) == LOW)
  {
    lastButtonTime = now;
    if (!isPlaying || currentFileName != "3.bin")
      startPlayback("/sd/3.bin");
  }
  else if (digitalRead(BUTTON_FILE_4) == LOW)
  {
    lastButtonTime = now;
    if (!isPlaying || currentFileName != "4.bin")
      startPlayback("/sd/4.bin");
  }
  else if (digitalRead(BUTTON_STOP) == LOW)
  {
    lastButtonTime = now;
    if (isPlaying)
      stopPlayback();
  }
}

void initSDCard()
{
  Serial.printf("[SD Pins] MISO=%d, MOSI=%d, SCK=%d, CS=%d\n", PIN_MISO, PIN_MOSI, PIN_SCK, PIN_CS);

  // SPI 핀 초기화
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);
  delay(100); // SPI 안정화 대기

  // SD 카드 마운트 시도
  Serial.println("  Attempting SD.begin()...");
  if (!SD.begin(PIN_CS, SPI, SD_SPI_HZ, "/sd", 5, false))
  {
    Serial.println("  [FAIL] SD.begin() failed! Check wiring or card.");
    Serial.println("  System will continue but cannot play files.");
    return;
  }

  // 카드 타입 확인
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE)
  {
    Serial.println("  [WARN] No SD card detected.");
    return;
  }

  const char *typeStr =
      (cardType == CARD_MMC) ? "MMC" : (cardType == CARD_SD) ? "SD"
                                   : (cardType == CARD_SDHC) ? "SDHC"
                                                             : "UNKNOWN";
  uint64_t sizeMB = SD.cardSize() / (1024ULL * 1024ULL);

  Serial.printf("  [OK] SD mounted. Type=%s, Size=%llu MB\n", typeStr, (unsigned long long)sizeMB);

  // 파일 목록 확인
  Serial.println("  Files available:");
  File root = SD.open("/sd");
  if (root)
  {
    File file = root.openNextFile();
    int count = 0;
    while (file && count < 10)
    {
      Serial.printf("    - %s\n", file.name());
      file = root.openNextFile();
      count++;
    }
    root.close();
  }
}

void startPlayback(const char *filePath)
{
  if (isPlaying)
    stopPlayback();

  Serial.println("\n--- File Playback Request ---");
  Serial.printf("[PLAY] Requested file: %s\n", filePath);

  // 여러 경로 시도
  const char *paths[3];
  char path1[50], path2[50], path3[50];

  // 파일명만 추출
  String fileName = String(filePath).substring(String(filePath).lastIndexOf('/') + 1);

  sprintf(path1, "/sd/%s", fileName.c_str()); // /sd/1.bin
  sprintf(path2, "/%s", fileName.c_str());    // /1.bin
  sprintf(path3, "%s", fileName.c_str());     // 1.bin

  paths[0] = path1;
  paths[1] = path2;
  paths[2] = path3;

  Serial.println("[DEBUG] Trying file paths:");
  bool opened = false;

  for (int i = 0; i < 3; i++)
  {
    Serial.printf("  [%d] %s... ", i + 1, paths[i]);
    binFile = SD.open(paths[i], FILE_READ);

    if (binFile)
    {
      Serial.println("✓ Found!");
      opened = true;
      break;
    }
    else
    {
      Serial.println("✗ Not found");
    }
  }

  if (!opened)
  {
    Serial.println("[ERROR] File not found in any path!");
    Serial.println("[TIP] Check if file exists on SD card:");

    // 실제 파일 목록 다시 표시
    File root = SD.open("/sd");
    if (root)
    {
      Serial.println("  Available files:");
      File file = root.openNextFile();
      while (file)
      {
        Serial.printf("    - %s\n", file.name());
        file = root.openNextFile();
      }
      root.close();
    }
    return;
  }

  // 파일 정보 읽기
  currentFileName = fileName;
  headerSize = sizeof(BinFileHeader);

  if (binFile.read((uint8_t *)&fileHeader, headerSize) != headerSize)
  {
    Serial.println("[ERROR] Failed to read header");
    binFile.close();
    return;
  }

  Serial.printf("[INFO] Width=%u, Height=%u, FPS=%.1f, Frames=%u\n",
                fileHeader.width, fileHeader.height, fileHeader.fps, fileHeader.frameCount);

  isPlaying = true;
  currentFrame = 0;
  lastFrameTime = millis();

  Serial.println("[START] Playback started!\n");
}

void stopPlayback()
{
  Serial.println("\n[STOP] Button pressed. Stopping playback...");
  isPlaying = false;
  currentFileName = "";
  if (binFile)
    binFile.close();

  FastLED.clear();
  FastLED.show();

  Serial.println("[STOPPED] All LEDs OFF");
}

void playNextFrame()
{
  unsigned long frameStart = millis();

  if (!binFile || !isPlaying)
    return;

  if (currentFrame >= fileHeader.frameCount)
  {
    Serial.println("\n[LOOP] Restarting playback...");
    currentFrame = 0;
    if (!binFile.seek(headerSize))
    {
      Serial.println("[ERROR] Failed to seek back to start.");
      stopPlayback();
      return;
    }
  }

  // Read data for all 12 physical strips and place their colors into the 2 chain buffers
  uint8_t stripData[LEDS_PER_STRIP * 3]; // 루프 밖으로 이동

  unsigned long sdReadStart = millis();
  unsigned long totalBytesRead = 0;

  for (int pin_idx = 0; pin_idx < NUM_PINS; pin_idx++)
  {
    for (int strip_in_chain = 0; strip_in_chain < STRIPS_PER_PIN; strip_in_chain++)
    {
      int bytesRead = binFile.read(stripData, sizeof(stripData));
      totalBytesRead += bytesRead;
      if (bytesRead != sizeof(stripData))
      {
        int physical_strip_num = pin_idx * STRIPS_PER_PIN + strip_in_chain;
        Serial.printf("[ERROR] Failed to read data for physical strip %d at frame %u.\n", physical_strip_num, currentFrame);
        stopPlayback();
        return;
      }

      int offset = strip_in_chain * LEDS_PER_STRIP;

      for (int row_in_strip = 0; row_in_strip < NUM_ROWS_PER_STRIP; row_in_strip++)
      {

        int absolute_row = strip_in_chain * NUM_ROWS_PER_STRIP + row_in_strip;

        for (int col = 0; col < LEDS_PER_ROW; col++)
        {
          int sourceDataIndex = (row_in_strip * LEDS_PER_ROW + col) * 3;
          int targetLedIndex_in_strip;

          if ((absolute_row % 2) == 1)
          { // Odd rows are reversed
            targetLedIndex_in_strip = (row_in_strip * LEDS_PER_ROW) + (LEDS_PER_ROW - 1 - col);
          }
          else
          { // Even rows are forward
            targetLedIndex_in_strip = (row_in_strip * LEDS_PER_ROW) + col;
          }

          uint8_t blue = stripData[sourceDataIndex];
          uint8_t green = stripData[sourceDataIndex + 1];
          uint8_t red = stripData[sourceDataIndex + 2];

          leds[pin_idx][offset + targetLedIndex_in_strip] = CRGB(red, green, blue);
        }
      }
    }
  }

  unsigned long sdReadTime = millis() - sdReadStart;

  // After all buffers are filled, show them all
  unsigned long showStart = millis();
  FastLED.show();
  unsigned long showTime = millis() - showStart;

  // 디버깅: SD 읽기 및 show() 시간 측정
  if (currentFrame % 30 == 0)
  {
    Serial.printf("[TIMING] SD Read: %lu ms (%lu KB, %.1f KB/s) | FastLED.show(): %lu ms\n",
                  sdReadTime, totalBytesRead / 1024,
                  (float)totalBytesRead / sdReadTime,
                  showTime);
  }

  // 디버깅: 첫 프레임의 첫 10개 LED 값 출력
  if (currentFrame == 0)
  {
    Serial.println("[DEBUG] First 10 LEDs of Chain 0:");
    for (int i = 0; i < 10; i++)
    {
      CRGB color = leds[0][i];
      Serial.printf("  LED[%d]: R=%3d G=%3d B=%3d", i, color.r, color.g, color.b);
      if (color.r > 0 || color.g > 0 || color.b > 0)
      {
        Serial.print(" ✓");
      }
      Serial.println();
    }
  }

  if (currentFrame % 30 == 0)
  {
    unsigned long totalFrameTime = millis() - frameStart;
    unsigned long frameInterval = millis() - lastFrameTime;
    float actualFPS = 1000.0 / frameInterval;
    float progress = (float)currentFrame / fileHeader.frameCount * 100.0;
    Serial.printf("[FRAME] %u/%u (%.1f%%) - Total: %lu ms | Interval: %lu ms (%.1f FPS)\n",
                  currentFrame, fileHeader.frameCount, progress, totalFrameTime, frameInterval, actualFPS);
  }
  currentFrame++;
}
