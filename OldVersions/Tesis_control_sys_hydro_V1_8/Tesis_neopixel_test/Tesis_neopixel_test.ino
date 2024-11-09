#include <Adafruit_NeoPixel.h>

#define NEOPIXELPIN 27
#define NUM_PIXELS 62

Adafruit_NeoPixel NeoPixel(NUM_PIXELS, NEOPIXELPIN, NEO_GRB + NEO_KHZ800);

void setup() {
  // put your setup code here, to run once:
  NeoPixel.begin();
}

void loop() {
  // put your main code here, to run repeatedly:
  NeoPixel.clear();  // set all pixel colors to 'off'. It only takes effect if pixels.show() is called

  // turn on all pixels to red at the same time for two seconds
  for (int pixel = 0; pixel < NUM_PIXELS; pixel++) {           // for each pixel
    NeoPixel.setPixelColor(2*pixel, NeoPixel.Color(255, 0, 0));  // it only takes effect if pixels.show() is called
    NeoPixel.setPixelColor((2*pixel)-1, NeoPixel.Color(0, 0, 255));  // it only takes effect if pixels.show() is called
  }
  NeoPixel.show();  // update to the NeoPixel Led Strip
  //delay(1000);      // 1 second on time
}
