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
extern "C"
{
#include "led_strip.h"
}

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
Strip::Strip(uint16_t n, uint8_t p, neoPixelType t, uint8_t ch)
{
 
}



