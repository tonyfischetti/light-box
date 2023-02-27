ARDUINO_DIR = /usr/share/arduino
ARDMK_DIR = ~/.arduino-makefile
BOARD_TAG = leonardo
ARCHITECTURE  = avr
# OR THE OTHER ONE?
MONITOR_PORT  = /dev/ttyACM0

ARDUINO_LIBS = Adafruit_NeoPixel elapsedMillis Wire LiquidCrystal_I2C IRremote

OPTIMIZATION_LEVEL=s

include $(ARDMK_DIR)/Arduino.mk
