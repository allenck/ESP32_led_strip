/*  ---------------------------------------------------------------------------
    File: led_strip.h
    Author(s):  Lucas Bruder <LBruder@me.com>
    Date Created: 11/23/2016
    Last modified: 11/26/2016
    Description: 
    This library can drive led strips through the RMT module on the ESP32.
    ------------------------------------------------------------------------ */

#ifndef LED_STRIP_H
#define LED_STRIP_H

#include <driver/rmt.h>
#include <driver/gpio.h>
#include "freertos/FreeRTOS.h"

#include <stddef.h>
#include <time.h>

// The order of primary colors in the NeoPixel data stream can vary among
// device types, manufacturers and even different revisions of the same
// item.  The third parameter to the Adafruit_NeoPixel constructor encodes
// the per-pixel byte offsets of the red, green and blue primaries (plus
// white, if present) in the data stream -- the following #defines provide
// an easier-to-use named version for each permutation. e.g. NEO_GRB
// indicates a NeoPixel-compatible device expecting three bytes per pixel,
// with the first byte transmitted containing the green value, second
// containing red and third containing blue. The in-memory representation
// of a chain of NeoPixels is the same as the data-stream order; no
// re-ordering of bytes is required when issuing data to the chain.
// Most of these values won't exist in real-world devices, but it's done
// this way so we're ready for it (also, if using the WS2811 driver IC,
// one might have their pixels set up in any weird permutation).

// Bits 5,4 of this value are the offset (0-3) from the first byte of a
// pixel to the location of the red color byte.  Bits 3,2 are the green
// offset and 1,0 are the blue offset.  If it is an RGBW-type device
// (supporting a white primary in addition to R,G,B), bits 7,6 are the
// offset to the white byte...otherwise, bits 7,6 are set to the same value
// as 5,4 (red) to indicate an RGB (not RGBW) device.
// i.e. binary representation:
// 0bWWRRGGBB for RGBW devices
// 0bRRRRGGBB for RGB

// RGB NeoPixel permutations; white and red offsets are always same
// Offset:         W        R        G        B
#define NEO_RGB  ((0<<6) | (0<<4) | (1<<2) | (2)) ///< Transmit as R,G,B
#define NEO_RBG  ((0<<6) | (0<<4) | (2<<2) | (1)) ///< Transmit as R,B,G
#define NEO_GRB  ((1<<6) | (1<<4) | (0<<2) | (2)) ///< Transmit as G,R,B
#define NEO_GBR  ((2<<6) | (2<<4) | (0<<2) | (1)) ///< Transmit as G,B,R
#define NEO_BRG  ((1<<6) | (1<<4) | (2<<2) | (0)) ///< Transmit as B,R,G
#define NEO_BGR  ((2<<6) | (2<<4) | (1<<2) | (0)) ///< Transmit as B,G,R

// RGBW NeoPixel permutations; all 4 offsets are distinct
// Offset:         W          R          G          B
#define NEO_WRGB ((0<<6) | (1<<4) | (2<<2) | (3)) ///< Transmit as W,R,G,B
#define NEO_WRBG ((0<<6) | (1<<4) | (3<<2) | (2)) ///< Transmit as W,R,B,G
#define NEO_WGRB ((0<<6) | (2<<4) | (1<<2) | (3)) ///< Transmit as W,G,R,B
#define NEO_WGBR ((0<<6) | (3<<4) | (1<<2) | (2)) ///< Transmit as W,G,B,R
#define NEO_WBRG ((0<<6) | (2<<4) | (3<<2) | (1)) ///< Transmit as W,B,R,G
#define NEO_WBGR ((0<<6) | (3<<4) | (2<<2) | (1)) ///< Transmit as W,B,G,R

#define NEO_RWGB ((1<<6) | (0<<4) | (2<<2) | (3)) ///< Transmit as R,W,G,B
#define NEO_RWBG ((1<<6) | (0<<4) | (3<<2) | (2)) ///< Transmit as R,W,B,G
#define NEO_RGWB ((2<<6) | (0<<4) | (1<<2) | (3)) ///< Transmit as R,G,W,B
#define NEO_RGBW ((3<<6) | (0<<4) | (1<<2) | (2)) ///< Transmit as R,G,B,W
#define NEO_RBWG ((2<<6) | (0<<4) | (3<<2) | (1)) ///< Transmit as R,B,W,G
#define NEO_RBGW ((3<<6) | (0<<4) | (2<<2) | (1)) ///< Transmit as R,B,G,W

#define NEO_GWRB ((1<<6) | (2<<4) | (0<<2) | (3)) ///< Transmit as G,W,R,B
#define NEO_GWBR ((1<<6) | (3<<4) | (0<<2) | (2)) ///< Transmit as G,W,B,R
#define NEO_GRWB ((2<<6) | (1<<4) | (0<<2) | (3)) ///< Transmit as G,R,W,B
#define NEO_GRBW ((3<<6) | (1<<4) | (0<<2) | (2)) ///< Transmit as G,R,B,W
#define NEO_GBWR ((2<<6) | (3<<4) | (0<<2) | (1)) ///< Transmit as G,B,W,R
#define NEO_GBRW ((3<<6) | (2<<4) | (0<<2) | (1)) ///< Transmit as G,B,R,W

#define NEO_BWRG ((1<<6) | (2<<4) | (3<<2) | (0)) ///< Transmit as B,W,R,G
#define NEO_BWGR ((1<<6) | (3<<4) | (2<<2) | (0)) ///< Transmit as B,W,G,R
#define NEO_BRWG ((2<<6) | (1<<4) | (3<<2) | (0)) ///< Transmit as B,R,W,G
#define NEO_BRGW ((3<<6) | (1<<4) | (2<<2) | (0)) ///< Transmit as B,R,G,W
#define NEO_BGWR ((2<<6) | (3<<4) | (1<<2) | (0)) ///< Transmit as B,G,W,R
#define NEO_BGRW ((3<<6) | (2<<4) | (1<<2) | (0)) ///< Transmit as B,G,R,W



// Add NEO_KHZ400 to the color order value to indicate a 400 KHz device.
// All but the earliest v1 NeoPixels expect an 800 KHz data stream, this is
// the default if unspecified. Because flash space is very limited on ATtiny
// devices (e.g. Trinket, Gemma), v1 NeoPixels aren't handled by default on
// those chips, though it can be enabled by removing the ifndef/endif below,
// but code will be bigger. Conversely, can disable the NEO_KHZ400 line on
// other MCUs to remove v1 support and save a little space.

#define NEO_KHZ800 0x0000 ///< 800 KHz data transmission
#ifndef __AVR_ATtiny85__
#define NEO_KHZ400 0x0100 ///< 400 KHz data transmission
#endif

// If 400 KHz support is enabled, the third parameter to the constructor
// requires a 16-bit value (in order to select 400 vs 800 KHz speed).
// If only 800 KHz is enabled (as is default on ATtiny), an 8-bit value
// is sufficient to encode pixel color order, saving some space.

#ifdef NEO_KHZ400
typedef uint16_t neoPixelType; ///< 3rd arg to Adafruit_NeoPixel constructor
#else
typedef uint8_t  neoPixelType; ///< 3rd arg to Adafruit_NeoPixel constructor
#endif

#if 0
enum rgb_led_type_t {
    RGB_LED_TYPE_WS2812 = 0,

    RGB_LED_TYPE_MAX,
};
#endif

/**
 * RGB LED colors
 */
struct led_color_t {
		uint8_t white;
 		uint8_t red;
		uint8_t green;
    uint8_t blue;
 }; 


// These two tables are declared outside the Adafruit_NeoPixel class
// because some boards may require oldschool compilers that don't
// handle the C++11 constexpr keyword.

/* A PROGMEM (flash mem) table containing 8-bit unsigned sine wave (0-255).
   Copy & paste this snippet into a Python REPL to regenerate:
import math
for x in range(256):
    print("{:3},".format(int((math.sin(x/128.0*math.pi)+1.0)*127.5+0.5))),
    if x&15 == 15: print
*/
static const uint8_t /*PROGMEM*/ _NeoPixelSineTable[256] = {
  128,131,134,137,140,143,146,149,152,155,158,162,165,167,170,173,
  176,179,182,185,188,190,193,196,198,201,203,206,208,211,213,215,
  218,220,222,224,226,228,230,232,234,235,237,238,240,241,243,244,
  245,246,248,249,250,250,251,252,253,253,254,254,254,255,255,255,
  255,255,255,255,254,254,254,253,253,252,251,250,250,249,248,246,
  245,244,243,241,240,238,237,235,234,232,230,228,226,224,222,220,
  218,215,213,211,208,206,203,201,198,196,193,190,188,185,182,179,
  176,173,170,167,165,162,158,155,152,149,146,143,140,137,134,131,
  128,124,121,118,115,112,109,106,103,100, 97, 93, 90, 88, 85, 82,
   79, 76, 73, 70, 67, 65, 62, 59, 57, 54, 52, 49, 47, 44, 42, 40,
   37, 35, 33, 31, 29, 27, 25, 23, 21, 20, 18, 17, 15, 14, 12, 11,
   10,  9,  7,  6,  5,  5,  4,  3,  2,  2,  1,  1,  1,  0,  0,  0,
    0,  0,  0,  0,  1,  1,  1,  2,  2,  3,  4,  5,  5,  6,  7,  9,
   10, 11, 12, 14, 15, 17, 18, 20, 21, 23, 25, 27, 29, 31, 33, 35,
   37, 40, 42, 44, 47, 49, 52, 54, 57, 59, 62, 65, 67, 70, 73, 76,
   79, 82, 85, 88, 90, 93, 97,100,103,106,109,112,115,118,121,124};

/* Similar to above, but for an 8-bit gamma-correction table.
   Copy & paste this snippet into a Python REPL to regenerate:
import math
gamma=2.6
for x in range(256):
    print("{:3},".format(int(math.pow((x)/255.0,gamma)*255.0+0.5))),
    if x&15 == 15: print
*/
static const uint8_t /*PROGMEM*/ _NeoPixelGammaTable[256] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  1,
    1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,  2,  3,  3,  3,  3,
    3,  3,  4,  4,  4,  4,  5,  5,  5,  5,  5,  6,  6,  6,  6,  7,
    7,  7,  8,  8,  8,  9,  9,  9, 10, 10, 10, 11, 11, 11, 12, 12,
   13, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19, 20,
   20, 21, 21, 22, 22, 23, 24, 24, 25, 25, 26, 27, 27, 28, 29, 29,
   30, 31, 31, 32, 33, 34, 34, 35, 36, 37, 38, 38, 39, 40, 41, 42,
   42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57,
   58, 59, 60, 61, 62, 63, 64, 65, 66, 68, 69, 70, 71, 72, 73, 75,
   76, 77, 78, 80, 81, 82, 84, 85, 86, 88, 89, 90, 92, 93, 94, 96,
   97, 99,100,102,103,105,106,108,109,111,112,114,115,117,119,120,
  122,124,125,127,129,130,132,134,136,137,139,141,143,145,146,148,
  150,152,154,156,158,160,162,164,166,168,170,172,174,176,178,180,
  182,184,186,188,191,193,195,197,199,202,204,206,209,211,213,215,
  218,220,223,225,227,230,232,235,237,240,242,245,247,250,252,255};

struct led_strip_t {
    //const enum rgb_led_type_t rgb_led_type;
    const neoPixelType rgb_led_type;
    uint32_t led_strip_length;

    // RMT peripheral settings
    rmt_channel_t rmt_channel;

    /*
     * Interrupt table is located in soc.h
     * As of 11/27/16, reccomended interrupts are:
     * 9, 12, 13, 17, 18, 19, 20, 21 or 23
     * Ensure that the same interrupt number isn't used twice
     * across all libraries
     */
    //int rmt_interrupt_num;

    gpio_num_t gpio; // Must be less than GPIO_NUM_33

    // Double buffering elements
    bool showing_buf_1;
    struct led_color_t *led_strip_buf_1;
    struct led_color_t *led_strip_buf_2; 

    SemaphoreHandle_t access_semaphore;
		uint8_t           rOffset;    ///< Red index within each 3- or 4-byte pixel
		uint8_t           gOffset;    ///< Index of green byte
		uint8_t           bOffset;    ///< Index of blue byte
		uint8_t 					wOffset;    ///< Index of white (==rOffset if no white)
    bool hasWhite;
};

clock_t millis();
void delay(int d);

bool led_strip_init(struct led_strip_t *led_strip);

/**
 * Sets the pixel at pixel_num to color.
 */
bool led_strip_set_pixel_color(struct led_strip_t *led_strip, uint32_t pixel_num, struct led_color_t *color);
bool led_strip_set_pixel_rgb(struct led_strip_t *led_strip, uint32_t pixel_num, uint8_t red, uint8_t green, uint8_t blue);
/**
 * Get the pixel color at pixel_num for the led strip that is currently being shown! 
 * NOTE: If you call set_pixel_color then get_pixel_color for the same pixel_num, you will not 
 * get back the same pixel value. This gets you the color of the pixel currently being shown, not the one
 * being updated
 *
 * If there is an invalid argument, color will point to NULL and this function will return false.
 */
bool led_strip_get_pixel_color(struct led_strip_t *led_strip, uint32_t pixel_num, struct led_color_t *color);

/**
 * Updates the led buffer to be shown using double buffering.
 */
bool led_strip_show(struct led_strip_t *led_strip);

/**
 * Clears the LED strip.
 */
bool led_strip_clear(struct led_strip_t *led_strip);

uint8_t    gamma8(uint8_t x);

uint32_t   Color(uint8_t r, uint8_t g, uint8_t b, uint8_t w);

void setPixelColor(struct led_strip_t *led_strip, uint16_t n, uint32_t c);

void fill(struct led_strip_t *led_strip, uint32_t c, uint16_t first, uint16_t count);

uint32_t ColorHSV(uint16_t hue, uint8_t sat/*=255*/, uint8_t val/*=255*/);

/*!
    @brief   A gamma-correction function for 32-bit packed RGB or WRGB
             colors. Makes color transitions appear more perceptially
             correct.
    @param   x  32-bit packed RGB or WRGB color.
    @return  Gamma-adjusted packed color, can then be passed in one of the
             setPixelColor() functions. Like gamma8(), this uses a fixed
             gamma correction exponent of 2.6, which seems reasonably okay
             for average NeoPixels in average tasks. If you need finer
             control you'll need to provide your own gamma-correction
             function instead.
  */
uint32_t gamma32(uint32_t x);

//uint8_t brightness; ///< Strip brightness 0-255 (stored as +1)

void main_led_task(void *args);
/*
 * clear buffer after show
 * if true, the buffer will be cleared after led_strip_show() is called.
 */
bool bClearOnShow;
#endif // LED_STRIP_H
