#include <FastLED.h>
#define NUM_LEDS 60
CRGB leds[NUM_LEDS];

void setup()
{
    FastLED.addLeds<WS2812, 3>(leds, NUM_LEDS);
}

void loop()
{
    leds[0] = CRGB::Red;
    FastLED.show();
    delay(500);
    leds[0] = CRGB::Blue;
    FastLED.show();
    delay(500);
}