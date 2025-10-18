#include <Adafruit_NeoPixel.h>

#define LED_PIN 3
#define NUM_LEDS 120 // 실제 연결된 LED 개수

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("\n==== LED Hardware Test ====");

    strip.begin();
    strip.clear();
    strip.show();
    strip.setBrightness(50);

    Serial.println("[TEST 1] All LEDs OFF (2 sec)");
    delay(2000);

    Serial.println("[TEST 2] First LED RED");
    strip.setPixelColor(0, 255, 0, 0);
    strip.show();
    delay(2000);

    Serial.println("[TEST 3] Second LED GREEN");
    strip.setPixelColor(1, 0, 255, 0);
    strip.show();
    delay(2000);

    Serial.println("[TEST 4] Third LED BLUE");
    strip.setPixelColor(2, 0, 0, 255);
    strip.show();
    delay(2000);

    Serial.println("[TEST 5] All three LEDs ON");
    delay(2000);

    Serial.println("[TEST 6] First 10 LEDs WHITE");
    for (int i = 0; i < 10; i++)
    {
        strip.setPixelColor(i, 255, 255, 255);
    }
    strip.show();
    delay(3000);

    Serial.println("[TEST 7] Rainbow pattern");
    for (int i = 0; i < min(NUM_LEDS, 30); i++)
    {
        strip.setPixelColor(i, strip.ColorHSV(i * 2184)); // 65536 / 30
    }
    strip.show();
    delay(3000);

    Serial.println("[TEST] Complete!");
}

void loop()
{
    // Chase effect
    static int pos = 0;

    strip.clear();
    for (int i = 0; i < 5; i++)
    {
        int idx = (pos + i) % NUM_LEDS;
        strip.setPixelColor(idx, 0, 0, 255);
    }
    strip.show();

    pos++;
    if (pos >= NUM_LEDS)
        pos = 0;

    delay(50);
}
