#include <FastLED.h>
#include <SPI.h>

// === Slave ID (각 Slave마다 다르게 설정) ===
const int SLAVE_ID = 1; // 1~12 중 하나로 설정

// === SPI Pin Settings ===
const int SPI_MOSI = 15; // Master에서 데이터 수신
const int SPI_SCK = 3;   // Clock
const int SPI_CS = 7;    // Chip Select (Master의 CS_PINS[x]와 연결)
const int SPI_SYNC = 16; // Sync signal (모든 Slave 공통)

// === LED Settings ===
const int LED_PIN = 8;           // WS2812 데이터 핀
const int LEDS_PER_STRIP = 1440; // 8 rows × 180 LEDs
const int LEDS_PER_ROW = 180;
const int NUM_ROWS = 8;

// === Global Variables ===
CRGB leds[LEDS_PER_STRIP];
uint8_t receiveBuffer[LEDS_PER_STRIP * 3];
volatile bool dataReady = false;
volatile int bufferIndex = 0;
volatile bool syncReceived = false;

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=====================================");
    Serial.printf("  Slave ESP32 #%d\n", SLAVE_ID);
    Serial.println("=====================================");

    // Initialize FastLED
    FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, LEDS_PER_STRIP);
    FastLED.setBrightness(100);
    FastLED.clear();
    FastLED.show();
    Serial.println("  ✓ FastLED initialized");

    // Initialize SPI as Slave
    pinMode(SPI_CS, INPUT);
    pinMode(SPI_SYNC, INPUT);

    SPI.begin(SPI_SCK, SPI_MOSI, -1, SPI_CS); // SCK, MOSI, MISO(none), SS
    SPI.setHwCs(true);

    // Attach interrupt for CS pin
    attachInterrupt(digitalPinToInterrupt(SPI_CS), onCSInterrupt, FALLING);
    attachInterrupt(digitalPinToInterrupt(SPI_SYNC), onSyncInterrupt, RISING);

    Serial.println("  ✓ SPI Slave initialized");
    Serial.println("\n  Waiting for Master...\n");
}

void loop()
{
    // Check if CS is active (LOW)
    if (digitalRead(SPI_CS) == LOW)
    {
        // Receive SPI data
        if (SPI.available())
        {
            uint8_t data = SPI.transfer(0x00);

            // Check for start marker
            if (bufferIndex == 0 && data == 0xAA)
            {
                bufferIndex = 0; // Ready to receive
            }
            else if (bufferIndex < sizeof(receiveBuffer))
            {
                receiveBuffer[bufferIndex++] = data;

                // Full frame received
                if (bufferIndex == sizeof(receiveBuffer))
                {
                    dataReady = true;
                    bufferIndex = 0;
                }
            }
        }
    }

    // Process received data
    if (dataReady)
    {
        processLEDData();
        dataReady = false;
    }

    // Show LEDs when sync signal received
    if (syncReceived)
    {
        FastLED.show();
        syncReceived = false;
    }
}

void onCSInterrupt()
{
    // CS went LOW - prepare to receive
    bufferIndex = 0;
}

void onSyncInterrupt()
{
    // SYNC signal - show LEDs
    syncReceived = true;
}

void processLEDData()
{
    // Convert BGR buffer to CRGB array with zigzag pattern
    for (int row = 0; row < NUM_ROWS; row++)
    {
        for (int col = 0; col < LEDS_PER_ROW; col++)
        {
            int sourceIndex = (row * LEDS_PER_ROW + col) * 3;
            int targetIndex;

            // Zigzag pattern (odd rows reversed)
            if (row % 2 == 1)
            {
                targetIndex = row * LEDS_PER_ROW + (LEDS_PER_ROW - 1 - col);
            }
            else
            {
                targetIndex = row * LEDS_PER_ROW + col;
            }

            uint8_t b = receiveBuffer[sourceIndex];
            uint8_t g = receiveBuffer[sourceIndex + 1];
            uint8_t r = receiveBuffer[sourceIndex + 2];

            leds[targetIndex] = CRGB(r, g, b);
        }
    }

    Serial.printf("[Slave #%d] Frame processed\n", SLAVE_ID);
}
