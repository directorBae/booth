#include <SPI.h>
#include <SD.h>

// === SD Card Pin Settings ===
const int PIN_MISO = 1;
const int PIN_MOSI = 6;
const int PIN_SCK = 7;
const int PIN_CS_SD = 8;
const uint32_t SD_SPI_HZ = 40000000;

// === SPI for Slaves ===
const int SPI_MOSI = 15; // Master Out Slave In
const int SPI_SCK = 3;   // Clock
const int SPI_SYNC = 16; // Sync signal (all slaves show() together)

// 12 Chip Select pins (one per slave)
const int CS_PINS[12] = {1, 2, 25, 11, 24, 23, 0, 27, 4, 5, 28, 12};

// === Button Pin Settings ===
const int BUTTON_FILE_1 = 10;
const int BUTTON_FILE_2 = 9;
const int BUTTON_FILE_3 = 13;
const int BUTTON_FILE_4 = 14;
const int BUTTON_STOP = 26;

// === LED Settings ===
const int LEDS_PER_ROW = 180;
const int NUM_ROWS = 100;
const int NUM_STRIPS = 12;
const int LEDS_PER_STRIP = 1440;
const int NUM_ROWS_PER_STRIP = 8;

// === File Header ===
struct BinFileHeader
{
    uint32_t width;
    uint32_t height;
    uint32_t fps;
    uint32_t frameCount;
};

// === Global Variables ===
SPIClass sdSPI(FSPI);
SPIClass slaveSPI(HSPI);
File binFile;
BinFileHeader fileHeader;
String currentFileName = "";
bool isPlaying = false;
uint32_t currentFrame = 0;
unsigned long lastFrameTime = 0;
unsigned long lastButtonTime = 0;
const unsigned long DEBOUNCE_DELAY = 200;
const size_t headerSize = 16;

void setup()
{
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n=====================================");
    Serial.println("  Multi-ESP32 LED Controller (Master)");
    Serial.println("=====================================");

    // STEP 1: Initialize buttons
    Serial.println("\n[STEP 1/3] Button Initialization");
    pinMode(BUTTON_FILE_1, INPUT_PULLUP);
    pinMode(BUTTON_FILE_2, INPUT_PULLUP);
    pinMode(BUTTON_FILE_3, INPUT_PULLUP);
    pinMode(BUTTON_FILE_4, INPUT_PULLUP);
    pinMode(BUTTON_STOP, INPUT_PULLUP);
    Serial.println("  ✓ Buttons initialized");

    // STEP 2: Initialize SD Card
    Serial.println("\n[STEP 2/3] SD Card Initialization");
    sdSPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS_SD);
    if (!SD.begin(PIN_CS_SD, sdSPI, SD_SPI_HZ))
    {
        Serial.println("  ✗ SD Card initialization FAILED!");
        while (1)
            delay(1000);
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("  ✓ SD Card initialized: %llu MB\n", cardSize);

    // STEP 3: Initialize SPI for Slaves
    Serial.println("\n[STEP 3/3] Slave SPI Initialization");

    // Set all CS pins as OUTPUT and HIGH (inactive)
    for (int i = 0; i < NUM_STRIPS; i++)
    {
        pinMode(CS_PINS[i], OUTPUT);
        digitalWrite(CS_PINS[i], HIGH);
    }

    pinMode(SPI_SYNC, OUTPUT);
    digitalWrite(SPI_SYNC, LOW);

    slaveSPI.begin(SPI_SCK, -1, SPI_MOSI, -1); // SCK, MISO(none), MOSI, SS(none)
    slaveSPI.setFrequency(20000000);           // 20 MHz

    Serial.println("  ✓ Slave SPI initialized (20 MHz)");

    Serial.println("\n=====================================");
    Serial.println("  Master Ready!");
    Serial.printf("  12 Slaves connected via SPI\n");
    Serial.println("=====================================");
}

void loop()
{
    checkButtons();

    if (isPlaying)
    {
        unsigned long now = millis();
        if (now - lastFrameTime >= 1) // Play as fast as possible
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
        stopPlayback();
    }
}

void startPlayback(const char *filename)
{
    stopPlayback();

    Serial.printf("\n[START] Opening file: %s\n", filename);

    binFile = SD.open(filename, FILE_READ);
    if (!binFile)
    {
        Serial.println("[ERROR] Failed to open file!");
        return;
    }

    // Read header
    uint8_t headerBuffer[headerSize];
    if (binFile.read(headerBuffer, headerSize) != headerSize)
    {
        Serial.println("[ERROR] Failed to read header!");
        binFile.close();
        return;
    }

    memcpy(&fileHeader.width, headerBuffer, 4);
    memcpy(&fileHeader.height, headerBuffer + 4, 4);
    memcpy(&fileHeader.fps, headerBuffer + 8, 4);
    memcpy(&fileHeader.frameCount, headerBuffer + 12, 4);

    Serial.printf("  Resolution: %dx%d\n", fileHeader.width, fileHeader.height);
    Serial.printf("  FPS: %d\n", fileHeader.fps);
    Serial.printf("  Frames: %d\n", fileHeader.frameCount);

    currentFileName = String(filename).substring(4); // Remove "/sd/"
    isPlaying = true;
    currentFrame = 0;
    lastFrameTime = millis();

    Serial.println("  ✓ Playback started!");
}

void stopPlayback()
{
    if (binFile)
        binFile.close();

    isPlaying = false;
    currentFrame = 0;
    currentFileName = "";

    // Send stop command to all slaves
    for (int i = 0; i < NUM_STRIPS; i++)
    {
        digitalWrite(CS_PINS[i], LOW);
        slaveSPI.transfer(0xFF); // Stop command
        digitalWrite(CS_PINS[i], HIGH);
    }

    Serial.println("\n[STOP] Playback stopped");
}

void playNextFrame()
{
    unsigned long frameStart = millis();

    if (!binFile || !isPlaying)
        return;

    // Loop playback
    if (currentFrame >= fileHeader.frameCount)
    {
        Serial.println("\n[LOOP] Restarting playback...");
        currentFrame = 0;
        if (!binFile.seek(headerSize))
        {
            Serial.println("[ERROR] Failed to seek!");
            stopPlayback();
            return;
        }
    }

    // Read and send data for all 12 strips
    uint8_t stripData[LEDS_PER_STRIP * 3];
    unsigned long sdReadStart = millis();

    for (int strip = 0; strip < NUM_STRIPS; strip++)
    {
        // Read strip data from SD
        int bytesRead = binFile.read(stripData, sizeof(stripData));
        if (bytesRead != sizeof(stripData))
        {
            Serial.printf("[ERROR] Failed to read strip %d data!\n", strip);
            stopPlayback();
            return;
        }

        // Send to corresponding slave via SPI
        digitalWrite(CS_PINS[strip], LOW);

        // Send start marker
        slaveSPI.transfer(0xAA);

        // Send LED data
        for (int i = 0; i < sizeof(stripData); i++)
        {
            slaveSPI.transfer(stripData[i]);
        }

        digitalWrite(CS_PINS[strip], HIGH);
    }

    unsigned long sdReadTime = millis() - sdReadStart;

    // Send SYNC signal to all slaves (show LEDs simultaneously)
    digitalWrite(SPI_SYNC, HIGH);
    delayMicroseconds(100);
    digitalWrite(SPI_SYNC, LOW);

    unsigned long totalTime = millis() - frameStart;

    // Progress info every 30 frames
    if (currentFrame % 30 == 0)
    {
        float fps = 1000.0 / totalTime;
        float progress = (float)currentFrame / fileHeader.frameCount * 100.0;
        Serial.printf("[FRAME] %u/%u (%.1f%%) - Time: %lu ms (%.1f FPS) | SD: %lu ms\n",
                      currentFrame, fileHeader.frameCount, progress, totalTime, fps, sdReadTime);
    }

    currentFrame++;
}
