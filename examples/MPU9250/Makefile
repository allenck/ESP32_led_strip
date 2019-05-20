#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := MPU9250

# Path to MPU Driver
EXTRA_COMPONENT_DIRS += $(abspath ../..)
# Path to I2Cbus

EXTRA_COMPONENT_DIRS += ${HOME}/esp/libraries/I2Cbus
EXTRA_COMPONENT_DIRS += $(abspath ../../components/)
EXCLUDE_COMPONENTS:= euler led_strip

include $(IDF_PATH)/make/project.mk

