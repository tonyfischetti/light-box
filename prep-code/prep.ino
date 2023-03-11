


#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>

/////
#include "Waveshare_LCD1602_RGB.h"

/* #define LCE_ADDRESS (0x7c >> 1) */

/* Waveshare_LCD1602_RGB lce(16,2);  //16 characters and 2 lines of show */
/////

LiquidCrystal_I2C lcd(0x3F, 16, 2);
RTC_DS3231 rtc;



DateTime now;
char the_date[11];
char the_time[9];
char the_temp[5];

char tmp_date[11];


unsigned long counter = 0;


/* void disp_the_date() { */
/*     strcpy(tmp_date, the_date); */
/*     byte data[3] = {0x40, 0}; */
/*     for (byte i = 0; i < 10; i++) { */
/*         data[2] = tmp_date[i]; */
/*         lce.write_char(tmp_date[i]); */
/*     } */
/* } */
/*  */
/* void disp_the_time() { */
/*     for (byte i = 0; i < 8; i++) { */
/*         lce.write_char(the_time[i]); */
/*         delay(100); */
/*     } */
/* } */


void setup() {

    Serial.begin(115200);

    if (!rtc.begin()) {
        Serial.println("Couldn't find RTC");
        Serial.flush();
        while (1) delay(10);
    }

    /*
    if (rtc.lostPower()) {
        Serial.println("RTC lost power, let's set the time!");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    */

    lcd.init();
    lcd.backlight();

    /* lce.init(); */
    /* lce.noAutoscroll(); */

    /* lce.setCursor(0,0); */
    /* lce.send_string(the_date); */
    /* lce.write_char('0'); */
    /* lce.write_char('1'); */
    /* lce.write_char('2'); */
    /* lce.setCursor(0,1); */
    /* lce.send_string("0123456789a"); */


}


void set_now() {
    now = rtc.now();
}

void set_date() {
    sprintf(the_date, "%d-%02d-%02d", now.year(), now.month(), now.day());
}

void set_time() {
    char am_or_pm[3] = "pm";
    int hour = now.hour();
    if (hour > 12) {
        am_or_pm[0] = 'p';
        hour = hour - 12;
    } else if (hour == 0) {
        hour = 12;
        am_or_pm[0] = 'a';
    } else {
        am_or_pm[0] = 'a';
    }
    sprintf(the_time, "%2d:%02d %s", hour, now.minute(), am_or_pm);
}

// TODO: update _way_ less often
void set_temp() {
    sprintf(the_temp, "%2ld C", round(rtc.getTemperature()));
}


void loop() {

    set_now();
    set_date();
    set_time();
    set_temp();

    /* Serial.println(now); */
    /* Serial.println(the_date); */
    /* Serial.println(the_time); */
    /* Serial.println(); */

    /*
    0123456789abcdef
      0123-55-89
    12:45
    */

    // TODO TODO: sometimes you'll have to clear it
    //            12:59 -> 1:00
    //            10 C -> 9 C
    // TODO TODO: put a degree character in!

    /* lcd.clear(); */
    lcd.setCursor(3, 0);
    lcd.print(the_date);
    lcd.setCursor(1, 1);
    lcd.print(the_time);
    lcd.setCursor(11, 1);
    lcd.print(the_temp);

    if (counter % 10 == 0) {
        Serial.print(the_date);
        Serial.print(" - ");
        Serial.println(the_time);
        Serial.println();
    }


    /* if (counter % 10 == 0) { */
    /*     lce.clear(); */
    /*     lce.setCursor(0, 0); */
    /*     lce.write_char('a'); */
    /*     disp_the_time(); */
    /* } */

    /* Serial.println(); */


    delay(500);
    /* delay(500); */
    /* delay(500); */

    ++counter;

}


