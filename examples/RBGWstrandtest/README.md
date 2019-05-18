# RGBWstrandtest Example

This example demonstrates the use of the led_strip C functions.

These functions are defined by including:
````C
#include "led_strip.h"

static struct led_color_t led_strip_buf_1[LED_STRIP_LENGTH];
static struct led_color_t led_strip_buf_2[LED_STRIP_LENGTH];
````

In your *app_main(void)* routine, place the following:
````C
int app_main(void)
{
    //nvs_flash_in;

     struct led_strip_t led_strip = {
        .rgb_led_type = NEO_RGB,         	// LED type (see led_strip.h" )
        .rmt_channel = CONFIG_RMT_CHANNEL,// RMT channel, 0 - 7
        .gpio = CONFIG_LED_STRIP_GPIO_PIN,// LED's GPIO pin
        .led_strip_buf_1 = led_strip_buf_1,
        .led_strip_buf_2 = led_strip_buf_2,
        .led_strip_length = LED_STRIP_LENGTH // number of pixels(LEDs) in strand
    };
    led_strip.access_semaphore = xSemaphoreCreateBinary();

    bool led_init_ok = led_strip_init(&led_strip);
    assert(led_init_ok);

    while (true) {

			loop(&led_strip);
		}
    return 0;
}
````
Provide a *loop(&led_strip)* function that will call the display functions to be executed in the loop.
````C
void loop(struct led_strip_t* led_strip) {
  struct led_color_t r = {.white = 0, .red = 255, .green = 0, .blue = 0};
  colorWipe(led_strip, &r, 50); // Red
   struct led_color_t g = { .red = 0, .green = 255, .blue = 0};
  colorWipe(led_strip, &g, 50); // Green
  struct led_color_t b = { .red = 0, .green = 0, .blue = 255};
  colorWipe(led_strip, &b, 50); // Blue
  if(led_strip->rOffset != led_strip->wOffset)
  {
		struct led_color_t w = { .red = 0, .green = 0, .blue = 0, .white = 255};
		colorWipe(led_strip, &w, 50); // True white (not RGB white)
	}

  whiteOverRainbow(led_strip, 75, 5);
  .
  .
  .
}
These display functions are modeled on similar functions in the Adafruit NeoPixel library. In most cases they require an additional parameter specifying a led_strip_t pointer.
````
