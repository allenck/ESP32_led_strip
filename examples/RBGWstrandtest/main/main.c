/*  ----------------------------------------------------------------------------
    File: main.c
    Author(s):  Lucas Bruder <LBruder@me.com>
    Date Created: 11/23/2016
    Last modified: 11/26/2016

    ------------------------------------------------------------------------- */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "esp_task.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "led_strip.h"

#include <stdio.h>
#include <time.h>
#include "esp_log.h"

#define TAG "main:"

#define LED_STRIP_LENGTH 22U
static struct led_color_t led_strip_buf_1[LED_STRIP_LENGTH];
static struct led_color_t led_strip_buf_2[LED_STRIP_LENGTH];

#define LED_STRIP_RMT_INTR_NUM 19
void loop(struct led_strip_t* led_strip);
void colorWipe(struct led_strip_t* led_strip, struct led_color_t* color, int wait);
void whiteOverRainbow(struct led_strip_t* led_strip, int whiteSpeed, int whiteLength);
void pulseWhite(struct led_strip_t* led_strip, uint8_t wait);
void rainbowFade2White(struct led_strip_t* led_strip, int wait, int rainbowLoops, int whiteLoops);


int app_main(void)
{
    //nvs_flash_in;

     struct led_strip_t led_strip = {
        .rgb_led_type = RGB_LED_TYPE_WS2812,
        .rmt_channel = RMT_CHANNEL_1,
        .rmt_interrupt_num = LED_STRIP_RMT_INTR_NUM,
        .gpio = GPIO_NUM_22,
        .led_strip_buf_1 = led_strip_buf_1,
        .led_strip_buf_2 = led_strip_buf_2,
        .led_strip_length = LED_STRIP_LENGTH
    };
    led_strip.access_semaphore = xSemaphoreCreateBinary();

    bool led_init_ok = led_strip_init(&led_strip);
    assert(led_init_ok);

    struct led_color_t led_color = {
        .red = 5,
        .green = 0,
        .blue = 0,
    };

#if 0
    while (true) {
        for (uint32_t index = 0; index < LED_STRIP_LENGTH; index++) {
            led_strip_set_pixel_color(&led_strip, index, &led_color);
        }
        led_strip_show(&led_strip);

        led_color.red += 5;
        vTaskDelay(30 / portTICK_PERIOD_MS);
}
#else
    loop(&led_strip);
#endif

    return 0;
}

void loop(struct led_strip_t* led_strip) {
  // Fill along the length of the strip in various colors...
  struct led_color_t r = { .red = 255, .green = 0, .blue = 0};
  colorWipe(led_strip, &r, 50); // Red
  struct led_color_t g = { .red = 0, .green = 255, .blue = 0};
  colorWipe(led_strip, &g, 50); // Green
  struct led_color_t b = { .red = 0, .green = 0, .blue = 255};
  colorWipe(led_strip, &b, 50); // Blue
  struct led_color_t w = { .red = 0, .green = 0, .blue = 0, .white = 255};
  colorWipe(led_strip, &w, 50); // True white (not RGB white)

  whiteOverRainbow(led_strip, 75, 5);

  pulseWhite(led_strip, 5);

  rainbowFade2White(led_strip, 3, 3, 1);

}

// Fill strip pixels one after another with a color. Strip is NOT cleared
// first; anything there will be covered pixel by pixel. Pass in color
// (as a single 'packed' 32-bit value, which you can get by calling
// strip.Color(red, green, blue) as shown in the loop() function above),
// and a delay time (in milliseconds) between pixels.
void colorWipe(struct led_strip_t* led_strip, struct led_color_t* color, int wait) {
  ESP_LOGI(TAG, "begin colorWipe %d,%d,%d,%d", color->red, color->green, color->blue, color->white);
  for(int i=0; i<LED_STRIP_LENGTH; i++) { // For each pixel in strip...
    led_strip_set_pixel_color(led_strip, i, color);         //  Set pixel's color (in RAM)
    led_strip_show(led_strip);                          //  Update strip to match
    vTaskDelay(wait);                           //  Pause for a moment
  }
}

void whiteOverRainbow(struct led_strip_t* led_strip, int whiteSpeed, int whiteLength) {
	ESP_LOGI(TAG, "begin whiteOverRainbow");
  //if(whiteLength >= strip.numPixels()) whiteLength = strip.numPixels() - 1;
  if(whiteLength >= led_strip->led_strip_length) whiteLength = led_strip->led_strip_length - 1;


  int      head          = whiteLength - 1;
  int      tail          = 0;
  int      loops         = 3;
  int      loopNum       = 0;
  clock_t lastTime      = millis();
  uint32_t firstPixelHue = 0;

  for(;;) 
  { // Repeat forever (or until a 'break' or 'return')
    for(int i=0; i<led_strip->led_strip_length; i++) 
    {  // For each pixel in strip...
      if(((i >= tail) && (i <= head)) ||      //  If between head & tail...
         ((tail > head) && ((i >= tail) || (i <= head))))
      {
        struct led_color_t w = { .red = 0, .green = 0, .blue = 0, .white = 255};
        led_strip_set_pixel_color(led_strip, i, &w); // Set white
      } else {                                             // else set rainbow
        int pixelHue = firstPixelHue + (i * 65536L / led_strip->led_strip_length);
        uint32_t c = gamma32(ColorHSV(pixelHue, 255, 255));
        struct led_color_t s = { .red = (c&0xff0000)>>16, .green = (c&0xff00)>>8, .blue = (c&0xff), .white = (c&0xff000000)>>24};
        led_strip_set_pixel_color(led_strip, i, &s);
      }
    }

    led_strip_show(led_strip); // Update strip with new contents
    // There's no delay here, it just runs full-tilt until the timer and
    // counter combination below runs out.

    firstPixelHue += 40; // Advance just a little along the color wheel
    ESP_LOGI(TAG, "loop time = %u",(uint16_t)(millis() - lastTime));
    if(((uint32_t)time(NULL) - lastTime) > whiteSpeed) 
    { // Time to update head/tail?
      if(++head >= led_strip->led_strip_length) 
      {      // Advance head, wrap around
        head = 0;
        if(++loopNum >= loops) return;
      }
      if(++tail >= led_strip->led_strip_length) 
      {      // Advance tail, wrap around
        tail = 0;
      }
      lastTime = millis();                   // Save time of last movement
    }
  }
}

void pulseWhite(struct led_strip_t* led_strip, uint8_t wait) {
  ESP_LOGI(TAG, "begin pulseWhite");
  for(int j=0; j<256; j++) { // Ramp up from 0 to 255
    // Fill entire strip with white at gamma-corrected brightness level 'j':
    struct led_color_t c = { .white = gamma8(j)};
    fill(led_strip, &c,0,0);
    led_strip_show(led_strip);
    vTaskDelay(wait);
  }

  for(int j=255; j>=0; j--) { // Ramp down from 255 to 0
    struct led_color_t c = { .white = gamma8(j)};
    fill(led_strip, &c,0,0);
    led_strip_show(led_strip);
    vTaskDelay(wait);
  }
}

void rainbowFade2White(struct led_strip_t* led_strip, int wait, int rainbowLoops, int whiteLoops) {
  ESP_LOGI(TAG, "begin rainbowFade2White");
  int fadeVal=0, fadeMax=100;

  // Hue of first pixel runs 'rainbowLoops' complete loops through the color
  // wheel. Color wheel has a range of 65536 but it's OK if we roll over, so
  // just count from 0 to rainbowLoops*65536, using steps of 256 so we
  // advance around the wheel at a decent clip.
  for(uint32_t firstPixelHue = 0; firstPixelHue < rainbowLoops*65536;
    firstPixelHue += 256) {

    for(int i=0; i<led_strip->led_strip_length; i++) { // For each pixel in strip...

      // Offset pixel hue by an amount to make one full revolution of the
      // color wheel (range of 65536) along the length of the strip
      // (strip.numPixels() steps):
      uint32_t pixelHue = firstPixelHue + (i * 65536L / led_strip->led_strip_length);

      // strip.ColorHSV() can take 1 or 3 arguments: a hue (0 to 65535) or
      // optionally add saturation and value (brightness) (each 0 to 255).
      // Here we're using just the three-argument variant, though the
      // second value (saturation) is a constant 255.
      uint32_t c = gamma32(ColorHSV(pixelHue, 255, 255* fadeVal / fadeMax));
        struct led_color_t s = { .red = (c&0xff0000)>>16, .green = (c&0xff00)>>8, .blue = (c&0xff), .white = (c&0xff000000)>>24};
      led_strip_set_pixel_color(led_strip, i, &s);
    }

    led_strip_show(led_strip);
    vTaskDelay(wait);

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
      struct led_color_t c = { .white = gamma8(j)};
      fill(led_strip, &c,0,0);
      led_strip_show(led_strip);;
    }
    vTaskDelay(1000); // Pause 1 second
    for(int j=255; j>=0; j--) { // Ramp down 255 to 0
      struct led_color_t c = { .white = gamma8(j)};
      fill(led_strip, &c,0,0);
      led_strip_show(led_strip);;
    }
  }

  vTaskDelay(500); // Pause 1/2 second
}

