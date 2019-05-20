/*  ---------------------------------------------------------------------------
    File: main.cpp
    Author(s):  Allen Kempe <allenck@windstream.net>
    Date Created: 05/14/2019
    Last modified: 05/14/2019
    Description: 
    C++ Example program using Strip class.
    ------------------------------------------------------------------------ */
#include "sdkconfig.h"
#include "strip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define TAG "main:"


void colorWipe(uint32_t color, int wait);
void whiteOverRainbow(int whiteSpeed, int whiteLength);
void pulseWhite(uint8_t wait);
void rainbowFade2White(int wait, int rainbowLoops, int whiteLoops);
void theaterChase(uint32_t color, int wait);
void rainbow(int wait);
void theaterChaseRainbow(int wait);
void loop();

Strip strip = Strip(CONFIG_LED_STRIP_NUM_PIXELS, CONFIG_LED_STRIP_GPIO_PIN, NEO_GRB + NEO_KHZ800, CONFIG_RMT_CHANNEL);
int main()
{
 strip.begin();
 
 while(true)
 {
  loop();
 }
 
 return 0;
}

void loop()
{
  colorWipe(Color(255,   0,   0).value(), 50);
  colorWipe(Color(0,   255,   0).value(), 50);
	colorWipe(Color(0,   0,   255).value(), 50);
  
  whiteOverRainbow(75, 5);

  pulseWhite( 5);

  rainbowFade2White(3, 3, 1);

  // Do a theater marquee effect in various colors...
  theaterChase(Color(0,127, 127, 127).value(), 50); // White, half brightness
  theaterChase(Color(0, 127, 0, 0).value(), 50); // Red, half brightness
  theaterChase(Color(0, 0, 0, 127).value(), 50); // Blue, half brightness

  rainbow(10);             // Flowing rainbow cycle along the whole strip
  theaterChaseRainbow(50); // Rainbow-enhanced theaterChase variant
 }

extern "C" void app_main()
{
 main();
}

// Fill strip pixels one after another with a color. Strip is NOT cleared
// first; anything there will be covered pixel by pixel. Pass in color
// (as a single 'packed' 32-bit value, which you can get by calling
// strip.Color(red, green, blue) as shown in the loop() function above),
// and a delay time (in milliseconds) between pixels.
void colorWipe(uint32_t color, int wait) {
  ESP_LOGI("colorWipe", "begin");
  for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...
    strip.setPixelColor(i, color);         //  Set pixel's color (in RAM)
    strip.show();                          //  Update strip to match
    delay(wait);                           //  Pause for a moment
  }
  ESP_LOGI("colorWipe", "end");
}

void whiteOverRainbow(int whiteSpeed, int whiteLength) {
	ESP_LOGI(TAG, "begin whiteOverRainbow");
  if(whiteLength >= strip.numPixels()) whiteLength = strip.numPixels() - 1;


  int      head          = whiteLength - 1;
  int      tail          = 0;
  int      loops         = 3;
  int      loopNum       = 0;
  clock_t lastTime      = millis();
  uint32_t firstPixelHue = 0;

  for(;;) 
  { // Repeat forever (or until a 'break' or 'return')
    for(int i=0; i<strip.numPixels(); i++) 
    {  // For each pixel in strip...
      if(((i >= tail) && (i <= head)) ||      //  If between head & tail...
         ((tail > head) && ((i >= tail) || (i <= head))))
      {
        strip.setPixelColor(i, Color(0,0,0,255).value()); // Set white
      } else {                                             // else set rainbow
        int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
        uint32_t c = strip.gamma32(strip.ColorHSV(pixelHue, 255, 255));
        strip.setPixelColor(i, c);
      }
    }

    strip.show(); // Update strip with new contents
    // There's no delay here, it just runs full-tilt until the timer and
    // counter combination below runs out.

    firstPixelHue += 40; // Advance just a little along the color wheel
    ESP_LOGD(TAG, "loop time = %u",(uint16_t)(millis() - lastTime));
    if(((uint32_t)time(NULL) - lastTime) > whiteSpeed) 
    { // Time to update head/tail?
      if(++head >= strip.numPixels()) 
      {      // Advance head, wrap around
        head = 0;
        if(++loopNum >= loops) return;
      }
      if(++tail >= strip.numPixels()) 
      {      // Advance tail, wrap around
        tail = 0;
      }
      lastTime = millis();                   // Save time of last movement
    }
  }
}

void pulseWhite(uint8_t wait) {
  ESP_LOGI(TAG, "begin pulseWhite");
  for(int j=0; j<256; j++) { // Ramp up from 0 to 255
    // Fill entire strip with white at gamma-corrected brightness level 'j':
    strip.fill(Color(strip.gamma8(j),strip.gamma8(j),strip.gamma8(j)).value(),0,0);
    strip.show();
    delay(wait);
  }
  ESP_LOGI(TAG, "ramp down pulseWhite");
  for(int j=255; j>=0; j--) { // Ramp down from 255 to 0
        strip.fill(Color(strip.gamma8(j),strip.gamma8(j),strip.gamma8(j)).value(),0,0);
    strip.show();
    delay(wait);
  }
  ESP_LOGI(TAG, "end pulseWhite");
}

void rainbowFade2White(int wait, int rainbowLoops, int whiteLoops) {
  ESP_LOGI(TAG, "begin rainbowFade2White");
  int fadeVal=0, fadeMax=100;

  // Hue of first pixel runs 'rainbowLoops' complete loops through the color
  // wheel. Color wheel has a range of 65536 but it's OK if we roll over, so
  // just count from 0 to rainbowLoops*65536, using steps of 256 so we
  // advance around the wheel at a decent clip.
  for(uint32_t firstPixelHue = 0; firstPixelHue < rainbowLoops*65536;
    firstPixelHue += 256) {

    for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...

      // Offset pixel hue by an amount to make one full revolution of the
      // color wheel (range of 65536) along the length of the strip
      // (strip.numPixels() steps):
      uint32_t pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());

      // strip.ColorHSV() can take 1 or 3 arguments: a hue (0 to 65535) or
      // optionally add saturation and value (brightness) (each 0 to 255).
      // Here we're using just the three-argument variant, though the
      // second value (saturation) is a constant 255.
      uint32_t c = strip.gamma32(strip.ColorHSV(pixelHue, 255, 255* fadeVal / fadeMax));
      strip.setPixelColor(i, c);
    }

    strip.show();
    delay(wait);

    if(firstPixelHue < 65536) {                              // First loop,
      if(fadeVal < fadeMax) fadeVal++;                       // fade in
    } else if(firstPixelHue >= ((rainbowLoops-1) * 65536)) { // Last loop,
      if(fadeVal > 0) fadeVal--;                             // fade out
    } else {
      fadeVal = fadeMax; // Interim loop, make sure fade is at max
    }
  }

  for(int k=0; k<whiteLoops; k++) {
    for(int j=0; j<256; j++) { // Ramp up 0 to 255
      // Fill entire strip with white at gamma-corrected brightness level 'j':
 			strip.fill(Color(strip.gamma8(j),strip.gamma8(j),strip.gamma8(j)).value(),0,0);
      strip.show();
    }
    delay(1000); // Pause 1 second
    for(int j=255; j>=0; j--) { // Ramp down 255 to 0
      strip.fill(Color(strip.gamma8(j),strip.gamma8(j),strip.gamma8(j)).value(),0,0);
      strip.show();
    }
  }

  delay(500); // Pause 1/2 second
}

// Theater-marquee-style chasing lights. Pass in a color (32-bit value,
// a la strip.Color(r,g,b) as mentioned above), and a delay time (in ms)
// between frames.
void theaterChase(uint32_t color, int wait) {
ESP_LOGI(TAG, "begin TheaterChase");
  for(int a=0; a<10; a++) {  // Repeat 10 times...
    for(int b=0; b<3; b++) { //  'b' counts from 0 to 2...
      strip.clear();         //   Set all pixels in RAM to 0 (off)
      // 'c' counts up from 'b' to end of strip in steps of 3...
      for(int c=b; c<strip.numPixels(); c += 3) 
			{
        strip.setPixelColor(c, color); // Set pixel 'c' to value 'color'
      }
      strip.show(); // Update strip with new contents
      delay(wait);  // Pause for a moment
    }
  }
}

// Rainbow cycle along whole strip. Pass delay time (in ms) between frames.
void rainbow(int wait) {
  ESP_LOGI(TAG, "begin rainbow");
  // Hue of first pixel runs 5 complete loops through the color wheel.
  // Color wheel has a range of 65536 but it's OK if we roll over, so
  // just count from 0 to 5*65536. Adding 256 to firstPixelHue each time
  // means we'll make 5*65536/256 = 1280 passes through this outer loop:
  for(long firstPixelHue = 0; firstPixelHue < 5*65536; firstPixelHue += 256) {
    for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...
      // Offset pixel hue by an amount to make one full revolution of the
      // color wheel (range of 65536) along the length of the strip
      // (strip.numPixels() steps):
      int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
      // strip.ColorHSV() can take 1 or 3 arguments: a hue (0 to 65535) or
      // optionally add saturation and value (brightness) (each 0 to 255).
      // Here we're using just the single-argument hue variant. The result
      // is passed through strip.gamma32() to provide 'truer' colors
      // before assigning to each pixel:
			uint32_t color = strip.gamma32(strip.ColorHSV(pixelHue, 255, 255));
      strip.setPixelColor(i, color);
    }
    strip.show(); // Update strip with new contents
    delay(wait);  // Pause for a moment
  }
}

// Rainbow-enhanced theater marquee. Pass delay time (in ms) between frames.
void theaterChaseRainbow(int wait) {
  ESP_LOGI(TAG, "begin theaterChaseRainbow");
  int firstPixelHue = 0;     // First pixel starts at red (hue 0)
  for(int a=0; a<30; a++) {  // Repeat 30 times...
    for(int b=0; b<3; b++) { //  'b' counts from 0 to 2...
      strip.clear();         //   Set all pixels in RAM to 0 (off)
      // 'c' counts up from 'b' to end of strip in increments of 3...
      for(int c=b; c<strip.numPixels(); c += 3) {
        // hue of pixel 'c' is offset by an amount to make one full
        // revolution of the color wheel (range 65536) along the length
        // of the strip (strip.numPixels() steps):
        int      hue   = firstPixelHue + c * 65536L / strip.numPixels();
        uint32_t color = strip.gamma32(strip.ColorHSV(hue, 255, 255)); // hue -> RGB
				strip.setPixelColor(c, color); // Set pixel 'c' to value 'color'
      }
      strip.show();                // Update strip with new contents
      delay(wait);                 // Pause for a moment
      firstPixelHue += 65536 / 90; // One cycle of color wheel over 90 frames
    }
  }
}
