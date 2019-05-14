#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

 #include <stdio.h>

#include "led_strip.h"

#define LED_STRIP_LENGTH CONFIG_LED_STRIP_NUM_PIXELS//22U
//#define LED_STRIP_RMT_INTR_NUM 19U
#define DELAY_MS 400
#define WIDTH 5
#define MAX_BRIGHTNESS 2

static struct led_color_t led_strip_buf_1[LED_STRIP_LENGTH];
static struct led_color_t led_strip_buf_2[LED_STRIP_LENGTH];


void main_led_task(void *args)
{
	struct led_strip_t led_strip = {
	    rgb_led_type : RGB_LED_TYPE_WS2812,
      led_strip_length : CONFIG_LED_STRIP_NUM_PIXELS, //LED_STRIP_LENGTH,
	    rmt_channel : CONFIG_RMT_CHANNEL, //RMT_CHANNEL_0,
	    /*rmt_interrupt_num : LED_STRIP_RMT_INTR_NUM,*/
	    gpio : CONFIG_LED_STRIP_GPIO_PIN, //GPIO_NUM_22,
			showing_buf_1 : false,
	    led_strip_buf_1 : led_strip_buf_1,
	    led_strip_buf_2 : led_strip_buf_2
	};
	led_strip.access_semaphore = xSemaphoreCreateBinary();

	bool led_init_ok = led_strip_init(&led_strip);
	assert(led_init_ok);
  //while(1){}
}
