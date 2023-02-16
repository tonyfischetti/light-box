
pro micro has:
  5 interrupt pins
  9 analog pins

SDA/SCL are spoken for because of the LCD screen
Adding an indicator LED will take any old pin
IR receiver will take another
A photoresistor will take an analog pin

Add a passive beeper

Add a RTC?

IN order to make an IR remote wqork with the knobs,
use an  `ANALOG_EPSILON` and, if the difference is
passes that threshold, `sensor_changed_p` flag goes
up and control goes back to the knobs
