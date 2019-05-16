/*  ---------------------------------------------------------------------------
    File: strip.h
    Author(s):  Allen Kempe <allenck@windstream.net>
    Date Created: 05/14/2019
    Last modified: 05/14/2019
    Description: 
    This library can drive led strips through the RMT module on the ESP32.
    ------------------------------------------------------------------------ */

#ifndef STRIP_H
#define STRIP_H

extern "C"
{
 #include "led_strip.h"
}

class Color
{
 public:
   Color(uint8_t red, uint8_t green, uint8_t blue, uint8_t white =0);
	 Color(uint32_t c);
	 uint32_t value();
	 uint8_t red();
	 uint8_t green();
	 uint8_t blue();
	 uint8_t white();
	private:
 	 uint32_t val = 0;
   
};

class Strip
{
 public:
 Strip(uint16_t n, uint8_t p=17, neoPixelType t=NEO_GRB+NEO_KHZ800, uint8_t ch=0);
 void main_led_task(void *args);

 private:
 struct led_strip_t* led_strip;
};
#endif
