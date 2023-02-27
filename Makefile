ARDUINO_DIR = /usr/share/arduino
ARDMK_DIR = ~/.arduino-makefile
BOARD_TAG = leonardo
ARCHITECTURE  = avr
MONITOR_PORT  = /dev/ttyACM0
AVRDUDE_CONF = /usr/share/arduino/hardware/tools/avrdude.conf

ARDUINO_LIBS = Adafruit_NeoPixel elapsedMillis Wire LiquidCrystal_I2C IRremote

OPTIMIZATION_LEVEL=s

include $(ARDMK_DIR)/Arduino.mk
