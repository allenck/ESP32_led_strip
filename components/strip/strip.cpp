/*  ---------------------------------------------------------------------------
    File: strip.cpp
    Author(s):  Allen Kempe <allenck@windstream.net>
    Date Created: 05/14/2019
    Last modified: 05/14/2019
    Description: 
    This library can drive led strips through the RMT module on the ESP32.
    ------------------------------------------------------------------------ */
#include "strip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

#define LED_STRIP_TASK_SIZE             (4096*2)
#define LED_STRIP_TASK_PRIORITY         (configMAX_PRIORITIES - 1)

#define LED_STRIP_REFRESH_PERIOD_MS     (30U) // TODO: add as parameter to led_strip_init

#define LED_STRIP_NUM_RMT_ITEMS_PER_LED (24U) // Assumes 24 bit color for each led

// RMT Clock source is @ 80 MHz. Dividing it by 8 gives us 10 MHz frequency, or 100ns period.
#define LED_STRIP_RMT_CLK_DIV (8)

#define TAG "Strip:"

Color::Color(uint8_t red, uint8_t green, uint8_t blue, uint8_t white)
{
 val = white << 24 | red << 16 | green << 8 | blue;
}
 Color::Color(uint32_t c) {}
 uint32_t Color::value() {return val;}
 uint8_t Color::red() {return val&0xff0000 >> 16;}
 uint8_t Color::green() {return val&0xff00 >> 8;}
 uint8_t Color::blue() {return val&0xff;}
 uint8_t Color::white() {return val&0xff000000 >> 24;}


/*
 * Constructor when length, pin and type are known at compile-time. 
*/
Strip::Strip(uint16_t n, uint8_t p, NeoPixelType t, uint8_t ch)
{
 _numPixels = n;
 gpio = p;
 neoPixelType = t;
 rmtChannel = ch;
}

bool Strip::begin()
{
	ESP_LOGI(TAG, "begin begin");
 	uint32_t b1[_numPixels];
  buf1 = b1;
  uint32_t b2[_numPixels];
  buf2 = b2;
  showingBuf1 = false;
  access_semaphore = xSemaphoreCreateBinary();
	wOffset = (neoPixelType >> 6) & 0b11; // See notes in header file
  rOffset = (neoPixelType >> 4) & 0b11; // regarding R/G/B/W offsets
  gOffset = (neoPixelType >> 2) & 0b11;
	bOffset = neoPixelType & 0b11;
  hasWhite = (wOffset != rOffset);
  memset(buf1, 0, sizeof(uint32_t)* _numPixels);
  memset(buf2, 0, sizeof(uint32_t)* _numPixels);

  if(!init_rmt())
   return false;

  xSemaphoreGive(access_semaphore);
  
  //ESP_LOG_BUFFER_HEXDUMP("begin buf1", buf1, sizeof(uint32_t) * _numPixels, ESP_LOG_INFO);
  //ESP_LOG_BUFFER_HEXDUMP("begin buf2", buf2, sizeof(uint32_t) * _numPixels, ESP_LOG_INFO);

  BaseType_t task_created = xTaskCreate(led_strip_task,
                                        "led_strip_task",
                                        LED_STRIP_TASK_SIZE,
                                        this,
                                        LED_STRIP_TASK_PRIORITY,
                                        &led_strip_task_handle
                                     );

  if (!task_created) {
   ESP_LOGI(TAG, "esp_strip_task not created");
      return false;
  }
  
  ESP_LOGI(TAG, "begin end");
  return true;
}

bool Strip::clearOnShow() {return bClearOnShow;}
void Strip::setClearOnShow(bool b) {bClearOnShow = b;}

bool Strip::init_rmt()
{
		ESP_LOGI("init_rmt", "begin");
		rmt_config_t rmt_cfg;
		rmt_cfg.rmt_mode =RMT_MODE_TX;							// rmt_mode_t rmt_mode
		rmt_cfg.channel = (rmt_channel_t)rmtChannel;// rmt_channel_t channel
		rmt_cfg.clk_div = LED_STRIP_RMT_CLK_DIV;		// uint8_t clk_div
		rmt_cfg.gpio_num =(gpio_num_t)gpio;					// gpio_num_t gpio_num
		rmt_cfg.mem_block_num = 1;												// mem_block_num
		//struct rmt_tx_config_t
		rmt_cfg.tx_config.loop_en = false; 								// bool loop_en
		rmt_cfg.tx_config.carrier_freq_hz =  100;					// uint32_t carrier_freq_hz Not used, but has to be set to avoid divide by 0 err
		rmt_cfg.tx_config.carrier_duty_percent = 50;			// uint8_t carrier_duty_percent
		rmt_cfg.tx_config.carrier_level = RMT_CARRIER_LEVEL_LOW; // rmt_carrier_level_t carrier_level
		rmt_cfg.tx_config.carrier_en = false;							// bool carrier_en
		rmt_cfg.tx_config.idle_level= RMT_IDLE_LEVEL_LOW; // rmt_idle_level_t idle_level
		rmt_cfg.tx_config.idle_output_en = true;					// bool idle_output_en
		 
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
		ESP_LOGI("init_rmt", "end");
    return true;
}

bool Strip::show()
{
  ESP_LOGI("show", "begin");
	bool success = true;

  xSemaphoreTake(access_semaphore, portMAX_DELAY);
  if (showingBuf1) {
    showingBuf1 = false;
 		if(bClearOnShow)
    	memset(buf1, 0, sizeof(uint32_t) * _numPixels);
    else
    {
     memcpy(buf1, buf2, sizeof(uint32_t) * _numPixels);
		}  
  } 
  else {
    showingBuf1 = true;
    if(bClearOnShow)
			memset(buf2, 0, sizeof(uint32_t) * _numPixels);
  else
    {
     memcpy(buf2, buf1, sizeof(uint32_t) * _numPixels);
		}
  }
  ESP_LOG_BUFFER_HEXDUMP("buf1", buf1, sizeof(uint32_t) * _numPixels, ESP_LOG_INFO);
  ESP_LOG_BUFFER_HEXDUMP("buf2", buf2, sizeof(uint32_t) * _numPixels, ESP_LOG_INFO);
	xSemaphoreGive(access_semaphore);

	ESP_LOGI("show", "end");
  return success;
}

/*static*/ void Strip::led_strip_task(void *arg)
{
  ESP_LOGI("led_strip_task", "Begin");
  Strip* strip = (Strip*)arg;  
    
  led_fill_rmt_items_fn led_make_waveform = NULL;
    bool make_new_rmt_items = true;
    bool prev_showing_buf_1 = !strip->showingBuf1;

    size_t num_items_malloc = (LED_STRIP_NUM_RMT_ITEMS_PER_LED * strip->_numPixels);
    rmt_item32_t *rmt_items = (rmt_item32_t*) malloc(sizeof(rmt_item32_t) * num_items_malloc);
    if (!rmt_items) {
        vTaskDelete(NULL);
    }
		ESP_LOGI("led_strip_task", "rmt_items allocated");
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
  //ESP_LOG_BUFFER_HEXDUMP("led_strip_task: buf1", strip->buf1, sizeof(uint32_t) * strip->_numPixels, ESP_LOG_INFO);
  //ESP_LOG_BUFFER_HEXDUMP("led_strip_task: buf2", strip->buf2, sizeof(uint32_t) * strip->_numPixels, ESP_LOG_INFO);
    for(;;) 
		{
        rmt_wait_tx_done((rmt_channel_t)strip->rmtChannel, portMAX_DELAY);
        xSemaphoreTake(strip->access_semaphore, portMAX_DELAY);

        /*
         * If buf 1 was previously being shown and now buf 2 is being shown,
         * it should update the new rmt items array. If buf 2 was previous being shown
         * and now buf 1 is being shown, it should update the new rmt items array.
         * Otherwise, no need to update the array
         */
        if ((prev_showing_buf_1 == true) && (strip->showingBuf1 == false)) {
            make_new_rmt_items = true;
        } else if ((prev_showing_buf_1 == false) && (strip->showingBuf1 == true)) {
            make_new_rmt_items = true;
        } else {
            make_new_rmt_items = false;
        }

        if (make_new_rmt_items) {
            if (strip->showingBuf1) {
                led_make_waveform(strip->buf1, rmt_items, strip->_numPixels,
									strip->wOffset, strip->rOffset, strip->gOffset, strip->bOffset);
            } else {
                led_make_waveform(strip->buf2, rmt_items, strip->_numPixels,
									strip->wOffset, strip->rOffset, strip->gOffset, strip->bOffset);
            }
        }

        rmt_write_items((rmt_channel_t)strip->rmtChannel, rmt_items, num_items_malloc, false);
        prev_showing_buf_1 = strip->showingBuf1;
        xSemaphoreGive(strip->access_semaphore);
        vTaskDelay(LED_STRIP_REFRESH_PERIOD_MS / portTICK_PERIOD_MS);
    }

    if (rmt_items) {
        free(rmt_items);
    }
    vTaskDelete(NULL);
}

/*static*/ void Strip::led_strip_fill_rmt_items_ws2812(uint32_t *led_strip_buf, rmt_item32_t *rmt_items, uint32_t led_strip_length, uint8_t wOffset, uint8_t rOffset,uint8_t gOffset,uint8_t bOffset)
{
		ESP_LOGI("led_strip_fill_rmt_items_ws2812", "begin");
    ESP_LOG_BUFFER_HEXDUMP(" begin buf", led_strip_buf, led_strip_length* sizeof(uint32_t), ESP_LOG_INFO);
    
    uint32_t rmt_items_index = 0;
    
    
    for (uint32_t led_index = 0; led_index < led_strip_length; led_index++) {
        uint32_t led_color = led_strip_buf[led_index];
        uint8_t bytes[4]= {(uint8_t)(led_color&0xff0000)>>16, (uint8_t)(led_color&0xff00)>>8, (uint8_t)(led_color&0xff), (uint8_t)(led_color&0xff)>>24};
        for (uint8_t bit = 8; bit != 0; bit--) {
            uint8_t bit_set = (bytes[wOffset] >> (bit - 1)) & 1;
            if(bit_set) {
                Strip::led_strip_rmt_bit_1_ws2812(&(rmt_items[rmt_items_index]));
            } else {
                Strip::led_strip_rmt_bit_0_ws2812(&(rmt_items[rmt_items_index]));
            }
            rmt_items_index++;
        }
        for (uint8_t bit = 8; bit != 0; bit--) {
            uint8_t bit_set = (bytes[gOffset] >> (bit - 1)) & 1;
            if(bit_set) {
                Strip::led_strip_rmt_bit_1_ws2812(&(rmt_items[rmt_items_index]));
            } else {
                Strip::led_strip_rmt_bit_0_ws2812(&(rmt_items[rmt_items_index]));
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
    ESP_LOG_BUFFER_HEXDUMP(" end buf", led_strip_buf, led_strip_length* sizeof(uint32_t), ESP_LOG_INFO);
		ESP_LOGI("led_strip_fill_rmt_items_ws2812", "end");	
}

bool Strip::setPixelColor(uint32_t pixel_num, uint32_t  color)
{
    bool set_led_success = true;

    if (showingBuf1) {
        buf2[pixel_num] = color;
    } else {
        buf1[pixel_num] = color;
    }

    return set_led_success;
}

/**
 * Clears the LED strip
 */
bool Strip::clear()
{
		ESP_LOGI(TAG, "begin clear");
    ESP_LOG_BUFFER_HEXDUMP("buf1", buf1, sizeof(uint32_t) * _numPixels, ESP_LOG_INFO);
    ESP_LOG_BUFFER_HEXDUMP("buf2", buf2, sizeof(uint32_t) * _numPixels, ESP_LOG_INFO);
    bool success = true;

    if (showingBuf1) {
        memset(buf2, 0, sizeof(uint32_t) * _numPixels);
    } else {
        memset(buf1, 0, sizeof(uint32_t) * _numPixels);
    }

    return success;
}

