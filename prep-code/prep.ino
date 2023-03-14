

// TODO TODO: it's a little behind. Can I manipulate __TIME__ to account
//            for the latency betwixt compilation and the installation
//            of the new firmware?


#include <elapsedMillis.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <TEA5767N.h>


#define DT_UPDATE_EVERY 500
#define RE_CHECK_EVERY  30

#define I2C_COLLECTOR 3
#define RE_DT_LAG     8
#define RE_CLK        9 // blue

#define DEBUG true


#define ST_Q 104.3
#define ST_HOT 97.1
#define ST_WQXR 105.9

#define NUM_STATIONS 3


LiquidCrystal_I2C lcd(0x3F, 16, 2);
RTC_DS3231 rtc;

TEA5767N radio = TEA5767N();

elapsedMillis dt_timer;
elapsedMillis re_timer;

DateTime now;
char the_date[11];
char the_time[9];
char the_temp[5];

char tmp_date[11];

unsigned long counter = 0;

byte current_station_index = 0;

float my_stations[3] = {ST_Q, ST_HOT, ST_WQXR};



void set_now() {
    now = rtc.now();
}

void set_date() {
    sprintf(the_date, "%d-%02d-%02d", now.year(), now.month(), now.day());
}

void set_time() {
    char am_or_pm[3] = "pm";
    int hour = now.hour();
    if (hour >= 12) {
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



void turn_radio_i2c_on() {
    digitalWrite(I2C_COLLECTOR, HIGH);
}

void turn_radio_i2c_off() {
    digitalWrite(I2C_COLLECTOR, LOW);
}


void update_rotary_encoder() {
    bool current_state_CLK = digitalRead(RE_CLK);
    static bool previous_state_CLK = current_state_CLK;
    bool current_state_DT;

    current_state_CLK = digitalRead(RE_CLK);
    if (current_state_CLK != previous_state_CLK  && current_state_CLK == 1){

        current_state_DT = digitalRead(RE_DT_LAG);
        // increment
        if (current_state_DT != current_state_CLK) {
            #if DEBUG
            Serial.println(F("supposed to increment"));
            #endif
            if (current_station_index == (NUM_STATIONS - 1))
                current_station_index = 0;
            else
                current_station_index++;
        }
        // decrement
        else {
            #if DEBUG
            Serial.println(F("supposed to decrement"));
            #endif
            if (current_station_index == 0)
                current_station_index = NUM_STATIONS - 1;
            else
                current_station_index--;
        }
        turn_radio_i2c_on();
        radio.selectFrequencyMuting(my_stations[current_station_index]);
        Serial.print("_now_ it's: ");
        Serial.println(my_stations[current_station_index]);
        Serial.print("strength: ");
        Serial.println(radio.getSignalLevel());
        Serial.println();

        turn_radio_i2c_off();
    }
    previous_state_CLK = current_state_CLK;
}



void setup() {

    pinMode(I2C_COLLECTOR, OUTPUT);
    pinMode(RE_CLK,        INPUT);
    pinMode(RE_DT_LAG,     INPUT);
    /* pinMode(RE_SW_BUTTON,  INPUT_PULLUP); */

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

    /* radio.setMonoReception(); */
    /*  */
    /* radio.setStereoNoiseCancellingOn(); */

    // TRANSISTORS!!
    turn_radio_i2c_on();
    radio.selectFrequencyMuting(my_stations[current_station_index]);
    Serial.println(radio.getSignalLevel());
    turn_radio_i2c_off();

}




void loop() {

    if (re_timer > RE_CHECK_EVERY) {
        update_rotary_encoder();
        re_timer = 0;
    }


    if (dt_timer > DT_UPDATE_EVERY) {

        /* Serial.println("triggered"); */

        set_now();
        set_date();
        set_time();
        set_temp();

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

        dt_timer = 0;
    }


    
    if (counter % 500001 == 0) {

        // Serial.print(the_date);
        // Serial.print(" - ");
        // Serial.println(the_time);
        // Serial.println();
        
        /* Serial.println("about to change stations"); */

        /* radio.startsSearchFrom(current_station); */
        /* delay(10); */
        /* current_station = radio.readFrequencyInMHz(); */
        /* Serial.print("now: "); */
        /* Serial.println(current_station); */

        // turn_radio_i2c_on();
        // if (counter % 2 == 0)
            // radio.selectFrequencyMuting(97.1);
        // else
            // radio.selectFrequencyMuting(104.3);
        // turn_radio_i2c_off();

        Serial.println("");

    }


    ++counter;

}


