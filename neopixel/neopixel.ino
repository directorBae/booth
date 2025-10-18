#include <Adafruit_NeoPixel.h>

// 레벨 시프터를 거쳐서 연결된 ESP32의 GPIO 핀 번호
#define LED_PIN   3

// 스트립에 연결된 LED의 총 개수
#define LED_COUNT 180

// Adafruit_NeoPixel 객체를 생성합니다.
// 파라미터: (LED 개수, 핀 번호, 픽셀 타입 및 속도)
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  strip.begin();           // NeoPixel 라이브러리 초기화
  strip.show();            // 모든 픽셀을 'off' 상태로 초기화
  strip.setBrightness(50); // 밝기 설정 (0-255), 너무 밝으면 전력 소모가 크니 주의
}

void loop() {
  // 첫 번째 픽셀을 빨간색으로 켭니다.
  for(int i=0;i<strip.numPixels();i++){
    strip.setPixelColor(i, strip.Color(255, 0, 0));
    } // R, G, B
    strip.show();
   // 변경된 색상 정보를 스트립에 전송
  delay(500);

  // 첫 번째 픽셀을 끕니다.
  for(int i=0;i<strip.numPixels();i++){
  strip.setPixelColor(i, strip.Color(0, 0, 0));strip.show();}
  delay(500);
}