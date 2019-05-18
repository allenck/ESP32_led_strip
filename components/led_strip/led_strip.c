/*  ----------------------------------------------------------------------------
    File: led_strip.c
    Author(s):  Lucas Bruder <LBruder@me.com>
    Date Created: 11/23/2016
    Last modified: 11/26/2016
    Description: LED Library for driving various led strips on ESP32.
    This library uses double buffering to display the LEDs.
    If the driver is showing buffer 1, any calls to led_strip_set_pixel_color
    will write to buffer 2. When it's time to drive the pixels on the strip, it
    refers to buffer 1. 
    When led_strip_show is called, it will switch to displaying the pixels
    from buffer 2 and will clear buffer 1. Any writes will now happen on buffer 1 
    and the task will look at buffer 2 for refreshing the LEDs
    ------------------------------------------------------------------------- */

#include "led_strip.h"
#include "freertos/task.h"

#include <string.h>
#include "esp_log.h"

#define TAG "led_strip:"

#define LED_STRIP_TASK_SIZE             (512)
#define LED_STRIP_TASK_PRIORITY         (configMAX_PRIORITIES - 1)

#define LED_STRIP_REFRESH_PERIOD_MS     (30U) // TODO: add as parameter to led_strip_init

#define LED_STRIP_NUM_RMT_ITEMS_PER_LED (24U) // Assumes 24 bit color for each led

// RMT Clock source is @ 80 MHz. Dividing it by 8 gives us 10 MHz frequency, or 100ns period.
#define LED_STRIP_RMT_CLK_DIV (8)

/****************************
        WS2812 Timing
 ****************************/
#define LED_STRIP_RMT_TICKS_BIT_1_HIGH_WS2812 9 // 900ns (900ns +/- 150ns per datasheet)
#define LED_STRIP_RMT_TICKS_BIT_1_LOW_WS2812  3 // 300ns (350ns +/- 150ns per datasheet)
#define LED_STRIP_RMT_TICKS_BIT_0_HIGH_WS2812 3 // 300ns (350ns +/- 150ns per datasheet)
#define LED_STRIP_RMT_TICKS_BIT_0_LOW_WS2812  9 // 900ns (900ns +/- 150ns per datasheet)

clock_t millis()
{
 clock_t t = clock() / (CLOCKS_PER_SEC / 1000);
 return t;
}

void delay(int d) { vTaskDelay(d / portTICK_PERIOD_MS);}

// Function pointer for generating waveforms based on different LED drivers
typedef void (*led_fill_rmt_items_fn)(struct led_color_t *led_strip_buf, rmt_item32_t *rmt_items, uint32_t led_strip_length, uint8_t wOffset, uint8_t rOffset,uint8_t gOffset,uint8_t bOffset);

static inline void led_strip_fill_item_level(rmt_item32_t* item, int high_ticks, int low_ticks)
{
    item->level0 = 1;
    item->duration0 = high_ticks;
    item->level1 = 0;
    item->duration1 = low_ticks;
}

static inline void led_strip_rmt_bit_1_ws2812(rmt_item32_t* item)
{
    led_strip_fill_item_level(item, LED_STRIP_RMT_TICKS_BIT_1_HIGH_WS2812, LED_STRIP_RMT_TICKS_BIT_1_LOW_WS2812);
}

static inline void led_strip_rmt_bit_0_ws2812(rmt_item32_t* item)
{
    led_strip_fill_item_level(item, LED_STRIP_RMT_TICKS_BIT_0_HIGH_WS2812, LED_STRIP_RMT_TICKS_BIT_0_LOW_WS2812);
}

static void led_strip_fill_rmt_items_ws2812(struct led_color_t *led_strip_buf, rmt_item32_t *rmt_items, uint32_t led_strip_length, uint8_t wOffset, uint8_t rOffset,uint8_t gOffset,uint8_t bOffset)
{
    uint32_t rmt_items_index = 0;
    
    
    for (uint32_t led_index = 0; led_index < led_strip_length; led_index++) {
        struct led_color_t led_color = led_strip_buf[led_index];
        uint8_t bytes[4]= {led_color.red, led_color.green, led_color.blue, led_color.white};
        for (uint8_t bit = 8; bit != 0; bit--) {
            uint8_t bit_set = (bytes[wOffset] >> (bit - 1)) & 1;
            if(bit_set) {
                led_strip_rmt_bit_1_ws2812(&(rmt_items[rmt_items_index]));
            } else {
                led_strip_rmt_bit_0_ws2812(&(rmt_items[rmt_items_index]));
            }
            rmt_items_index++;
        }
        for (uint8_t bit = 8; bit != 0; bit--) {
            uint8_t bit_set = (bytes[gOffset] >> (bit - 1)) & 1;
            if(bit_set) {
                led_strip_rmt_bit_1_ws2812(&(rmt_items[rmt_items_index]));
            } else {
                led_strip_rmt_bit_0_ws2812(&(rmt_items[rmt_items_index]));
            }
            rmt_items_index++;
        }
        for (uint8_t bit = 8; bit != 0; bit--) {
            uint8_t bit_set = (bytes[bOffset] >> (bit - 1)) & 1;
            if(bit_set) {
                led_strip_rmt_bit_1_ws2812(&(rmt_items[rmt_items_index]));
            } else {
                led_strip_rmt_bit_0_ws2812(&(rmt_items[rmt_items_index]));
            }
            rmt_items_index++;
        }
        if(wOffset != rOffset)
        { 
		      for (uint8_t bit = 8; bit != 0; bit--) {
		          uint8_t bit_set = (bytes[wOffset] >> (bit - 1)) & 1;
		          if(bit_set) {
		              led_strip_rmt_bit_1_ws2812(&(rmt_items[rmt_items_index]));
		          } else {
		              led_strip_rmt_bit_0_ws2812(&(rmt_items[rmt_items_index]));
		          }
		          rmt_items_index++;
		      }
       }
    }
}

static void led_strip_task(void *arg)
{
    struct led_strip_t *led_strip = (struct led_strip_t *)arg;
    led_fill_rmt_items_fn led_make_waveform = NULL;
    bool make_new_rmt_items = true;
    bool prev_showing_buf_1 = !led_strip->showing_buf_1;

    size_t num_items_malloc = (LED_STRIP_NUM_RMT_ITEMS_PER_LED * led_strip->led_strip_length);
    rmt_item32_t *rmt_items = (rmt_item32_t*) malloc(sizeof(rmt_item32_t) * num_items_malloc);
    if (!rmt_items) {
        vTaskDelete(NULL);
    }
#if 0
    switch (led_strip->rgb_led_type) {
        case RGB_LED_TYPE_WS2812:
            led_make_waveform = led_strip_fill_rmt_items_ws2812;
            break;

        default:
            // Will avoid keeping it point to NULL
            led_make_waveform = led_strip_fill_rmt_items_ws2812;
            break;
    };
#else
		led_make_waveform = led_strip_fill_rmt_items_ws2812;
#endif
    for(;;) {
        rmt_wait_tx_done(led_strip->rmt_channel, portMAX_DELAY);
        xSemaphoreTake(led_strip->access_semaphore, portMAX_DELAY);

        /*
         * If buf 1 was previously being shown and now buf 2 is being shown,
         * it should update the new rmt items array. If buf 2 was previous being shown
         * and now buf 1 is being shown, it should update the new rmt items array.
         * Otherwise, no need to update the array
         */
        if ((prev_showing_buf_1 == true) && (led_strip->showing_buf_1 == false)) {
            make_new_rmt_items = true;
        } else if ((prev_showing_buf_1 == false) && (led_strip->showing_buf_1 == true)) {
            make_new_rmt_items = true;
        } else {
            make_new_rmt_items = false;
        }

        if (make_new_rmt_items) {
            if (led_strip->showing_buf_1) {
                led_make_waveform(led_strip->led_strip_buf_1, rmt_items, led_strip->led_strip_length,
									led_strip->wOffset, led_strip->rOffset, led_strip->gOffset,led_strip->bOffset);
            } else {
                led_make_waveform(led_strip->led_strip_buf_2, rmt_items, led_strip->led_strip_length,
									led_strip->wOffset, led_strip->rOffset, led_strip->gOffset,led_strip->bOffset);
            }
        }

        rmt_write_items(led_strip->rmt_channel, rmt_items, num_items_malloc, false);
        prev_showing_buf_1 = led_strip->showing_buf_1;
        xSemaphoreGive(led_strip->access_semaphore);
        vTaskDelay(LED_STRIP_REFRESH_PERIOD_MS / portTICK_PERIOD_MS);
    }

    if (rmt_items) {
        free(rmt_items);
    }
    vTaskDelete(NULL);
}

static bool led_strip_init_rmt(struct led_strip_t *led_strip)
{
    rmt_config_t rmt_cfg = {
        .rmt_mode = RMT_MODE_TX,
        .channel = led_strip->rmt_channel,
        .clk_div = LED_STRIP_RMT_CLK_DIV,
        .gpio_num = led_strip->gpio,
        .mem_block_num = 1,
        .tx_config = {
            .loop_en = false,
            .carrier_freq_hz = 100, // Not used, but has to be set to avoid divide by 0 err
            .carrier_duty_percent = 50,
            .carrier_level = RMT_CARRIER_LEVEL_LOW,
            .carrier_en = false,
            .idle_level = RMT_IDLE_LEVEL_LOW,
            .idle_output_en = true,
        }
    };
    ESP_LOGI(TAG, "rmt_driver_install gpio= %d, channel= %d",rmt_cfg.gpio_num, rmt_cfg.channel);
    esp_err_t cfg_ok = rmt_config(&rmt_cfg);
    if (cfg_ok != ESP_OK) {
      ESP_LOGI(TAG, "rmt_config failed, gpio = %d", rmt_cfg.gpio_num);
        return false;
    }
#if 1
    esp_err_t install_ok = rmt_driver_install(rmt_cfg.channel, 0, 0);
    if (install_ok != ESP_OK) {
        ESP_LOGI(TAG, "rmt_driver_install failed, gpio= %d, channel= %d, error = %s",rmt_cfg.gpio_num, rmt_cfg.channel, esp_err_to_name(install_ok));
     return false;
    }
#endif
    return true;
}

bool led_strip_init(struct led_strip_t *led_strip)
{
    TaskHandle_t led_strip_task_handle;

    if ((led_strip == NULL) ||
        (led_strip->rgb_led_type == 0 || led_strip->rgb_led_type > 0x3ff) ||
        (led_strip->rmt_channel == RMT_CHANNEL_MAX) ||
        (led_strip->gpio > GPIO_NUM_33) ||  // only inputs above 33
        (led_strip->led_strip_buf_1 == NULL) ||
        (led_strip->led_strip_buf_2 == NULL) ||
        (led_strip->led_strip_length == 0) ||
        (led_strip->access_semaphore == NULL)) {
        ESP_LOGE(TAG, "led_strip invalid, rgb_led_type=%d", led_strip->rgb_led_type);
        return false;
    }
		// calculate byte order offsets 
    led_strip->wOffset = (led_strip->rgb_led_type >> 6) & 0b11; // See notes in header file
    led_strip->rOffset = (led_strip->rgb_led_type >> 4) & 0b11; // regarding R/G/B/W offsets
  	led_strip->gOffset = (led_strip->rgb_led_type >> 2) & 0b11;
		led_strip->bOffset = led_strip->rgb_led_type & 0b11;
    led_strip->hasWhite = (led_strip->rOffset != led_strip->wOffset);

    if(led_strip->led_strip_buf_1 == led_strip->led_strip_buf_2) {
     ESP_LOGI(TAG, "led_strip_buf1 invalid");
        return false;
    }

    memset(led_strip->led_strip_buf_1, 0, sizeof(struct led_color_t) * led_strip->led_strip_length);
    memset(led_strip->led_strip_buf_2, 0, sizeof(struct led_color_t) * led_strip->led_strip_length);

    bool init_rmt = led_strip_init_rmt(led_strip);
    if (!init_rmt) {
	   return false;
    }

    xSemaphoreGive(led_strip->access_semaphore);
    BaseType_t task_created = xTaskCreate(led_strip_task,
                                            "led_strip_task",
                                            LED_STRIP_TASK_SIZE,
                                            led_strip,
                                            LED_STRIP_TASK_PRIORITY,
                                            &led_strip_task_handle
                                         );

    if (!task_created) {
     ESP_LOGI(TAG, "esp_strip_task not created");
        return false;
    }

    return true;
}

bool led_strip_set_pixel_color(struct led_strip_t *led_strip, uint32_t pixel_num, struct led_color_t  *color)
{
    bool set_led_success = true;

    if ((!led_strip) || (!color) || (pixel_num > led_strip->led_strip_length)) {
        return false;
    }

    if (led_strip->showing_buf_1) {
        led_strip->led_strip_buf_2[pixel_num] = *color;
    } else {
        led_strip->led_strip_buf_1[pixel_num] = *color;
    }

    return set_led_success;
}

bool led_strip_set_pixel_rgb(struct led_strip_t *led_strip, uint32_t pixel_num, uint8_t red, uint8_t green, uint8_t blue)
{
    bool set_led_success = true;

    if ((!led_strip) || (pixel_num > led_strip->led_strip_length)) {
        return false;
    }

    if (led_strip->showing_buf_1) {
        led_strip->led_strip_buf_2[pixel_num].red = red;
        led_strip->led_strip_buf_2[pixel_num].green = green;
        led_strip->led_strip_buf_2[pixel_num].blue = blue;
    } else {
        led_strip->led_strip_buf_1[pixel_num].red = red;
        led_strip->led_strip_buf_1[pixel_num].green = green;
        led_strip->led_strip_buf_1[pixel_num].blue = blue;
    }

    return set_led_success;
}

bool led_strip_get_pixel_color(struct led_strip_t *led_strip, uint32_t pixel_num, struct led_color_t *color)
{
    bool get_success = true;

    if ((!led_strip) ||
        (pixel_num > led_strip->led_strip_length) ||
        (!color)) {
        color = NULL;
        return false;
    }

    if (led_strip->showing_buf_1) {
        *color = led_strip->led_strip_buf_1[pixel_num];
    } else {
        *color = led_strip->led_strip_buf_2[pixel_num];
    }

    return get_success;
}

/**
 * Updates the led buffer to be shown
 */
bool led_strip_show(struct led_strip_t *led_strip)
{
  bool success = true;

  if (!led_strip) {
  	return false;
  }

  xSemaphoreTake(led_strip->access_semaphore, portMAX_DELAY);
  if (led_strip->showing_buf_1) {
    led_strip->showing_buf_1 = false;
 		if(bClearOnShow)
    	memset(led_strip->led_strip_buf_1, 0, sizeof(struct led_color_t) * led_strip->led_strip_length);
    else
    {
     memcpy(led_strip->led_strip_buf_1, led_strip->led_strip_buf_2, sizeof(struct led_color_t) * led_strip->led_strip_length);
		}  
  } else {
    led_strip->showing_buf_1 = true;
    if(bClearOnShow)
			memset(led_strip->led_strip_buf_2, 0, sizeof(struct led_color_t) * led_strip->led_strip_length);
  else
    {
     memcpy(led_strip->led_strip_buf_2, led_strip->led_strip_buf_1, sizeof(struct led_color_t) * led_strip->led_strip_length);
		} }
  xSemaphoreGive(led_strip->access_semaphore);

  return success;
}

/**
 * Clears the LED strip
 */
bool led_strip_clear(struct led_strip_t *led_strip)
{
		ESP_LOGD(TAG, "begin clear");
    bool success = true;

    if (!led_strip) {
        return false;
    }

    if (led_strip->showing_buf_1) {
        memset(led_strip->led_strip_buf_2, 0, sizeof(struct led_color_t) * led_strip->led_strip_length);
    } else {
        memset(led_strip->led_strip_buf_1, 0, sizeof(struct led_color_t) * led_strip->led_strip_length);
    }

    return success;
}

/*!
    @brief   An 8-bit gamma-correction function for basic pixel brightness
             adjustment. Makes color transitions appear more perceptially
             correct.
    @param   x  Input brightness, 0 (minimum or off/black) to 255 (maximum).
    @return  Gamma-adjusted brightness, can then be passed to one of the
             setPixelColor() functions. This uses a fixed gamma correction
             exponent of 2.6, which seems reasonably okay for average
             NeoPixels in average tasks. If you need finer control you'll
             need to provide your own gamma-correction function instead.
  */
uint8_t    gamma8(uint8_t x) {
    //ESP_LOGI(TAG, "gamma in = %d, out = %d", (int)x, (int)(_NeoPixelGammaTable[x]));
    return /*pgm_read_byte*/(_NeoPixelGammaTable[x]); // 0-255 in, 0-255 out
}

/*!
    @brief   Convert separate red, green, blue and white values into a
             single "packed" 32-bit WRGB color.
    @param   r  Red brightness, 0 to 255.
    @param   g  Green brightness, 0 to 255.
    @param   b  Blue brightness, 0 to 255.
    @param   w  White brightness, 0 to 255.
    @return  32-bit packed WRGB value, which can then be assigned to a
             variable for later use or passed to the setPixelColor()
             function. Packed WRGB format is predictable, regardless of
             LED strand color order.
 */
uint32_t   Color(uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
    return ((uint32_t)w << 24) | ((uint32_t)r << 16) | ((uint32_t)g <<  8) | b;
}

/*!
  @brief   Fill all or part of the NeoPixel strip with a color.
  @param   c      32-bit color value. Most significant byte is white (for
                  RGBW pixels) or ignored (for RGB pixels), next is red,
                  then green, and least significant byte is blue. If all
                  arguments are unspecified, this will be 0 (off).
  @param   first  Index of first pixel to fill, starting from 0. Must be
                  in-bounds, no clipping is performed. 0 if unspecified.
  @param   count  Number of pixels to fill, as a positive value. Passing
                  0 or leaving unspecified will fill to end of strip.
*/
void fill(struct led_strip_t *led_strip, uint32_t c, uint16_t first, uint16_t count) {
  uint16_t i, end;

  if(first >= led_strip->led_strip_length) {
    return; // If first LED is past end of strip, nothing to do
  }

  // Calculate the index ONE AFTER the last pixel to fill
  if(count == 0) {
    // Fill to end of strip
    end = led_strip->led_strip_length;
  } else {
    // Ensure that the loop won't go past the last pixel
    end = first + count;
    if(end > led_strip->led_strip_length) end = led_strip->led_strip_length;
  }

  
  for(i = first; i < end; i++) {
    struct led_color_t t = { .red = (c&0xff0000)>>16, .green = (c&0xff00)>>8, .blue = (c&0xff), .white = (c&0xff000000)>>24};
   led_strip_set_pixel_color(led_strip, i, &t);
  }
}

uint32_t ColorHSV(uint16_t hue, uint8_t sat, uint8_t val) {

 uint8_t r, g, b;
 // Remap 0-65535 to 0-1529. Pure red is CENTERED on the 64K rollover;
  // 0 is not the start of pure red, but the midpoint...a few values above
  // zero and a few below 65536 all yield pure red (similarly, 32768 is the
  // midpoint, not start, of pure cyan). The 8-bit RGB hexcone (256 values
  // each for red, green, blue) really only allows for 1530 distinct hues
  // (not 1536, more on that below), but the full unsigned 16-bit type was
  // chosen for hue so that one's code can easily handle a contiguous color
  // wheel by allowing hue to roll over in either direction.
  hue = (hue * 1530L + 32768) / 65536;
  // Because red is centered on the rollover point (the +32768 above,
  // essentially a fixed-point +0.5), the above actually yields 0 to 1530,
  // where 0 and 1530 would yield the same thing. Rather than apply a
  // costly modulo operator, 1530 is handled as a special case below.

  // So you'd think that the color "hexcone" (the thing that ramps from
  // pure red, to pure yellow, to pure green and so forth back to red,
  // yielding six slices), and with each color component having 256
  // possible values (0-255), might have 1536 possible items (6*256),
  // but in reality there's 1530. This is because the last element in
  // each 256-element slice is equal to the first element of the next
  // slice, and keeping those in there this would create small
  // discontinuities in the color wheel. So the last element of each
  // slice is dropped...we regard only elements 0-254, with item 255
  // being picked up as element 0 of the next slice. Like this:
  // Red to not-quite-pure-yellow is:        255,   0, 0 to 255, 254,   0
  // Pure yellow to not-quite-pure-green is: 255, 255, 0 to   1, 255,   0
  // Pure green to not-quite-pure-cyan is:     0, 255, 0 to   0, 255, 254
  // and so forth. Hence, 1530 distinct hues (0 to 1529), and hence why
  // the constants below are not the multiples of 256 you might expect.

  // Convert hue to R,G,B (nested ifs faster than divide+mod+switch):
  if(hue < 510) {         // Red to Green-1
    b = 0;
    if(hue < 255) {       //   Red to Yellow-1
      r = 255;
      g = hue;            //     g = 0 to 254
    } else {              //   Yellow to Green-1
      r = 510 - hue;      //     r = 255 to 1
      g = 255;
    }
  } else if(hue < 1020) { // Green to Blue-1
    r = 0;
    if(hue <  765) {      //   Green to Cyan-1
      g = 255;
      b = hue - 510;      //     b = 0 to 254
    } else {              //   Cyan to Blue-1
      g = 1020 - hue;     //     g = 255 to 1
      b = 255;
    }
  } else if(hue < 1530) { // Blue to Red-1
    g = 0;
    if(hue < 1275) {      //   Blue to Magenta-1
      r = hue - 1020;     //     r = 0 to 254
      b = 255;
    } else {              //   Magenta to Red-1
      r = 255;
      b = 1530 - hue;     //     b = 255 to 1
    }
  } else {                // Last 0.5 Red (quicker than % operator)
    r = 255;
    g = b = 0;
  }

  // Apply saturation and value to R,G,B, pack into 32-bit result:
  uint32_t v1 =   1 + val; // 1 to 256; allows >>8 instead of /255
  uint16_t s1 =   1 + sat; // 1 to 256; same reason
  uint8_t  s2 = 255 - sat; // 255 to 0
  return ((((((r * s1) >> 8) + s2) * v1) & 0xff00) << 8) |
          (((((g * s1) >> 8) + s2) * v1) & 0xff00)       |
         ( ((((b * s1) >> 8) + s2) * v1)           >> 8);
}

// A 32-bit variant of gamma8() that applies the same function
// to all components of a packed RGB or WRGB value.
uint32_t gamma32(uint32_t x) {
  uint8_t *y = (uint8_t *)&x;
  // All four bytes of a 32-bit value are filtered even if RGB (not WRGB),
  // to avoid a bunch of shifting and masking that would be necessary for
  // properly handling different endianisms (and each byte is a fairly
  // trivial operation, so it might not even be wasting cycles vs a check
  // and branch for the RGB case). In theory this might cause trouble *if*
  // someone's storing information in the unused most significant byte

  // of an RGB value, but this seems exceedingly rare and if it's
  // encountered in reality they can mask values going in or coming out.
  for(uint8_t i=0; i<4; i++) y[i] = gamma8(y[i]);
  return x; // Packed 32-bit return
}

struct led_strip_t* led_strip = NULL;

void main_led_task(void *args)
{

 struct led_color_t led_strip_buf_1[CONFIG_LED_STRIP_NUM_PIXELS];
 struct led_color_t led_strip_buf_2[CONFIG_LED_STRIP_NUM_PIXELS];
 
 struct led_strip_t new_led_strip =  {
	    rgb_led_type : NEO_RGB,
      led_strip_length : CONFIG_LED_STRIP_NUM_PIXELS, //LED_STRIP_LENGTH,
	    rmt_channel : (rmt_channel_t)CONFIG_RMT_CHANNEL, //RMT_CHANNEL_0,
	    /*rmt_interrupt_num : LED_STRIP_RMT_INTR_NUM,*/
	    gpio : (gpio_num_t)CONFIG_LED_STRIP_GPIO_PIN, //GPIO_NUM_22,
			showing_buf_1 : false,
	    led_strip_buf_1 : led_strip_buf_1,
	    led_strip_buf_2 : led_strip_buf_2
	};
  led_strip = &new_led_strip;
#if 0
	if(args != NULL)
  {
   led_strip = (struct led_strip_t*)args; 
  }
#endif
  //led_strip->led_strip_length = CONFIG_LED_STRIP_NUM_PIXELS;
  ESP_LOG_BUFFER_HEXDUMP(TAG, led_strip, 160, ESP_LOG_INFO);
  led_strip->access_semaphore = xSemaphoreCreateBinary();
  

  bool led_init_ok = led_strip_init(led_strip);
	assert(led_init_ok);

  struct led_color_t led_color = {
        white : 0,
        red : 5,
        green : 5,
        blue : 0
  };
  while (true) 
  {
    ESP_LOGI(TAG, "begin loop");
        for (uint32_t index = 0; index < led_strip->led_strip_length; index++) {
            led_strip_set_pixel_color(led_strip, index, &led_color);
        }
        ESP_LOG_BUFFER_HEXDUMP("led_strip", led_strip, sizeof(struct led_strip_t), ESP_LOG_INFO);
        ESP_LOG_BUFFER_HEXDUMP("led_buf_1", led_strip->led_strip_buf_1, sizeof(uint32_t)*led_strip->led_strip_length, ESP_LOG_INFO);
        ESP_LOG_BUFFER_HEXDUMP("led_buf_2", led_strip->led_strip_buf_2, sizeof(uint32_t)*led_strip->led_strip_length, ESP_LOG_INFO);
        
        led_strip_show(led_strip);

        led_color.red += 5;
        
        vTaskDelay(30 / portTICK_PERIOD_MS);
  }
}
