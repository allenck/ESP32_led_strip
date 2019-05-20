# MPU9250 example

This example runs on an ESP32 TTGO Audio boad that has a MPU9250 Gyroscope Compass 9-Axis Sensor and a string of 22 WS2812 RGB Leds. The color of the LEDs is determined by the orientation of the board.

# Prerequisites
To support the MPU9250 3 axis sensor, https://github.com/natanaeljr/esp32-MPU-driver  must be installed. These modules must be downloaded into the components directory. ![MPU driver API](https://natanaeljr.github.io/esp32-MPU-driver/html/index.html) This library also requires either the I2Cbus or SPIbus be present in $(home)/esp/libraries.
