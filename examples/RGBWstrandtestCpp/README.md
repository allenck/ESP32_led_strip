# RGBWstrandtestCpp Example

This example demonstrates the use of the C++ class Strip to drive a string of WS2812 LEDs.

In your CPP file. you must include
````C
#include "strip.h"
````
Now create an instance of class 'Strip' and a function called 'main':
````C
Strip strip = Strip(CONFIG_LED_STRIP_NUM_PIXELS, CONFIG_LED_STRIP_GPIO_PIN, NEO_GRB + NEO_KHZ800, CONFIG_RMT_CHANNEL);
int main()
{
 strip.begin();
 
 
 return 0;
}
Note the variables *CONFIG_LED_STRIP_NUM_PIXELS*, *CONFIG_LED_STRIP_GPIO_PIN*. etc which have been created by `make menuconfig` per the main/Kconfig.projbuild file.
````
Then create a function called 'main':":
````C
extern "C" void app_main()
{
 main();
}
````

You must provide a function called 'loop' which will call various LED display functions of your choosing. Several are defined in this example.:
````C
void loop()
{
  colorWipe(Color(255,   0,   0).value(), 50);
  colorWipe(Color(0,   255,   0).value(), 50);
	colorWipe(Color(0,   0,   255).value(), 50);
  
  whiteOverRainbow(75, 5);

  pulseWhite( 5);

  .
  .
  .
}
````
See the examples in the Adafruit NeoPixel library for additional display functions. The Strip class contains various functions like clear, setPixelColor, fill, gamma8, gamma32, ColorHSV, etc, that are named the same as functions in the Adafruit NeoPixel library.

