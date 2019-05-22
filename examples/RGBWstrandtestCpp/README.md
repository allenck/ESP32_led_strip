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

## Using QtCreator

This project contains a QtCreator project (RGBWstrandtestCpp.pro) file which can be used to open the project in QtCreator. See [Template esp-idf project for Qt Creator](https://github.com/ascii78/esp-template-qtcreator) and
execute the steps outlined in *Build/Deploy*. These steps are reproduced here:

## Build/Deploy

The build/deploy settings for Qt Creator are stored in the *.pro.user file, I
could not get a generic one to include in this template. The manual steps are:

1. Go to "Projects" in the sidebar
2. Select "Build" to select the "Build settings" if it's not yet selected.
3. Under general, disable 'shadow build'
4. Under build steps, remove everything and "Add build step->Make" with argument "all"
5. Under clean steps, remove everything and "Add build step->Make" with argument "clean"
6. Select "Run" to select the "Run settings" if it's not yet selected.A
7. Under deployment, select "Add Deploy step->Make" with argument "flash"
8. Under run, remove everything and add "Custom Executable" with "/usr/bin/cmake",
9. arguments "monitor", and working directory: "%{CurrentProject:Path}", check "run
10. in terminal"
11. Check if clean and build works in the menu and sidebar, run should now default to
    a make clean and run the serial monitor in a terminal.
