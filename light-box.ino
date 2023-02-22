#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <elapsedMillis.h>

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

/********************************************************
 * POTENTIAL IMPROVEMENTS                               *
 *                                                      *
 *   - Have a pattern with "uniform" brightness         *
 *   - Use `static` where I can                         *
 *   - Use `const` where I can                          *
 *   - Can I use complicated macros to reduce           *
 *     repetition?                                      *
 *   - Test different gamma values                      *
 *   - Have another pattern (like 1) but avoids         *
 *     spending too much time on chartreuse             *
 *   - Use different resistors for each channel,        *
 *     (255, 255, 255) should be neutral white          *
 *     (not very light cyan)                            *
 *                                                      *
 ********************************************************/


/* ---------------------------------------------------------
 * PROGMEM (so we can put it at the bottom)               */

extern const byte gamma_xlate[];



/* ---------------------------------------------------------
 * MACROS                                                 */

#define DEBUG   false
#define PROFILE true

// Total number of available Neopixel (even if they're not all used)
#define ALL_NP_COUNT 16

// value to change the color by on each step
// #define STEP_DELTA 5
#define STEP_DELTA 2

// number of milliseconds to wait for buttons, etc. to settle
#define EPSILON 250

// The indices into the `current_rgbw` array for each color
#define RED_INDEX   0
#define GREEN_INDEX 1
#define BLUE_INDEX  2
#define WHITE_INDEX 3

#define DBUG_EVERY      3000
#define POT_CHECK_EVERY 50
#define RE_CHECK_EVERY  10
#define LCD_EVERY       500

#define NUM_PATTERNS 4

// fix
#define UP        true
#define DOWN      false
#define INCREMENT true
#define DECREMENT false

// TODO TODO: temp (or is it?)
// TODO TODO: what the hell does this mean, anyway?
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))

/* ---------------------------------------------------------
 * PIN MACROS                                             */

/* - - FOR PRO MICRO - - - - - - -*/
// TODO TODO have another set for other board(s)

#define SDA           2   // orange
#define SCL           3   // periwinkle
#define BUZZER        4   // dark pink
#define NEOPIXEL_PIN  5   // sea green
#define PHOTORESISTOR A7  // (6) yellow
#define RE_SW_BUTTON  7   // navy
#define RE_DT_LAG     8   // purple
#define RE_CLK        9   // green

#define IR_PIN        10  // light pink

#define THUMB_POT_0_IN  A0
#define THUMB_POT_1_IN  A1
#define THUMB_POT_2_IN  A2


/* - - FOR ARDUINO UNO - - - - - - -*/
/*
#define NEOPIXEL_PIN  7

#define RE_CLK        13
#define RE_DT_LAG     12
#define RE_SW_BUTTON  11

#define THUMB_POT_0_IN  A0
#define THUMB_POT_1_IN  A1
#define THUMB_POT_2_IN  A2
*/
/* I2C AND IC2 PINS -- */
// TODO TODO: #define LCD_I2C_ADDRESS
// #define SDA  A4
// #define SCL  A5



/* ---------------------------------------------------------
 * GLOBALS                                                */

// Number of NeoPixel LEDS (8 in the stick)
byte NP_COUNT = 8;

// holds the NeoPixel object
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(ALL_NP_COUNT, NEOPIXEL_PIN,
                                             NEO_GRBW + NEO_KHZ800);

// holds the values for each RGBw channel (globally)
byte current_rgbw[4] = {255, 255, 255, 0};

// TODO: will this be a sensor value function??
bool gamma_correct_p = true;

// flag that gets set when the pattern is changed
bool pattern_changed_p = false;

#if PROFILE
// TODO: does it have to be a long?
unsigned long current_fun_inner_loop_time = 0;
#endif

// timers for various things that need timers
elapsedMillis debug_timer;
elapsedMillis pot_timer;
elapsedMillis re_timer;
elapsedMillis step_timer;
elapsedMillis lcd_timer;

#if PROFILE
elapsedMillis inner_loop_time;
#endif

LiquidCrystal_I2C lcd(0x27, 20, 4);



/* ---------------------------------------------------------
 * SENSOR VALUE VARIABLES                                 */

// holds the index (into pattern_functions) for the current
// pattern's function pointer
// Used everywhere (rotary encoder)
byte current_pattern_fun_index = 1;

// used by patterns {0, 1, 2} (potentiometer)
byte brightness = 255;

// used by patterns {0, 1} (potentiometer)
byte step_delay_0 = 70;

// used by patterns {0, 1} (potentiometer)
byte strobe_delay_0 = 0;


/* ---------------------------------------------------------
 * IS THIS HIGHER ORDER PROGRAMMING? IN C?!               */

void nothing_function () {
    return;
}

typedef void (*SensorUpdateFunction) ();
SensorUpdateFunction update_thumb_pot_0 = nothing_function;
SensorUpdateFunction update_thumb_pot_1 = nothing_function;
SensorUpdateFunction update_thumb_pot_2 = nothing_function;
SensorUpdateFunction sw_button_press    = nothing_function;

typedef void (*PatternFunction) ();
PatternFunction current_pattern_function = nothing_function;

typedef void (*OutputUpdateFunction) ();
OutputUpdateFunction update_LCD         = nothing_function;



/* ---------------------------------------------------------
 * DEBUG AND PROFILING                                    */

// TODO: more debug info (pattern, etc...)
bool debug_values() {
    #if DEBUG
    if (debug_timer > DBUG_EVERY) {
        Serial.print("Pattern:    ");
        Serial.println(current_pattern_fun_index);
        Serial.print("RGBW:       ");
        Serial.print(current_rgbw[RED_INDEX]);
        Serial.print(", ");
        Serial.print(current_rgbw[GREEN_INDEX]);
        Serial.print(", ");
        Serial.print(current_rgbw[BLUE_INDEX]);
        Serial.print(", ");
        Serial.println(current_rgbw[WHITE_INDEX]);
        Serial.print("brightness: ");
        Serial.println(brightness);
        Serial.print("gamma:      ");
        Serial.println(gamma_correct_p);
        Serial.print("step_timer:  ");
        Serial.println(step_timer);
        Serial.print("debug timer: ");
        Serial.println(debug_timer);
        Serial.print("pot timer:   ");
        Serial.println(pot_timer);
        Serial.print("re_timer:    ");
        Serial.println(re_timer);
        Serial.print("step delay: ");
        Serial.println(step_delay_0);
        Serial.println("-----------------");
        debug_timer = 0;
    }
    #endif
    return true;
}

int free_ram() {
    extern int __heap_start, *__brkval;
    int v;
    return (int)&v - (__brkval == 0  ? (int)&__heap_start : (int) __brkval);
}



/* ---------------------------------------------------------
 * SENSOR UPDATE FUNCTIONS                                 */

void update_current_pattern_fun_index(bool increment_p) {
    if (increment_p){
        if (current_pattern_fun_index == (NUM_PATTERNS - 1)){
            current_pattern_fun_index = 0;
        } else {
            current_pattern_fun_index++;
        }
    } else {
        if (current_pattern_fun_index == 0) {
            current_pattern_fun_index = NUM_PATTERNS - 1;
        }
        else {
            current_pattern_fun_index--;
        }
    }
    pattern_changed_p = true;

}

// This one stays the same for all patterns
bool update_rotary_encoder() {
    bool current_state_CLK = digitalRead(RE_CLK);
    static bool previous_state_CLK = current_state_CLK;
    bool current_state_DT;

    current_state_CLK = digitalRead(RE_CLK);
    if (current_state_CLK != previous_state_CLK  && current_state_CLK == 1){
        current_state_DT = digitalRead(RE_DT_LAG);
        #if DEBUG
        Serial.println("current_state_CLK is different than before");
        Serial.print("PREVIOUS CLK:    ");
        Serial.println(previous_state_CLK);
        Serial.print("CURRENT_CLK:     ");
        Serial.println(current_state_CLK);
        Serial.print("CURRENT DT:     ");
        Serial.println(current_state_DT);
        #endif
        if (current_state_DT != current_state_CLK) {
            #if DEBUG
            Serial.println("supposed to increment");
            #endif
            update_current_pattern_fun_index(true);
        } else {
            #if DEBUG
            Serial.println("supposed to decrement");
            #endif
            update_current_pattern_fun_index(false);
        }
    }
    previous_state_CLK = current_state_CLK;
    return !pattern_changed_p;
}

// This one stays the same for all patterns
// If the button is pressed, it calls `sw_button_press`, which
// _can_ change based on the pattern / setting
void update_re_button() {
    static long previous_sw_button_press;
    long current_millis = millis();
    if (digitalRead(RE_SW_BUTTON)==LOW) {
        if ((current_millis - previous_sw_button_press) > EPSILON){
            #if DEBUG
            Serial.println("button clicked. about to run `sw_button_press`");
            #endif
            sw_button_press();
        }
        previous_sw_button_press = current_millis;
    }
}

// Used by patterns {0, 1, 2, 4}
void update_brightness() {
    // TODO TODO: store (static) previous brightness, and
    // allowance for IR integration
    brightness = map(analogRead(THUMB_POT_0_IN), 0, 1023, 255, 1);
    pixels.setBrightness(brightness);
}

// Used by patterns {0, 1}
void update_step_delay() {
    step_delay_0 = map(analogRead(THUMB_POT_1_IN), 0, 1023, 255, 1);
}

// TODO TODO: should I just get rid of this?
// Used by patterns {0, 1, 2}
void update_strobe_delay() {
    strobe_delay_0 = map(analogRead(THUMB_POT_2_IN), 0, 1023, 255, 0);
}

// TODO now: reverse these

// Used by patterns {2}
void update_red_brightness() {
    current_rgbw[RED_INDEX] = map(analogRead(THUMB_POT_0_IN),
                                  0, 1023, 255, 0);
}

// Used by patterns {2}
void update_green_brightness() {
    current_rgbw[GREEN_INDEX] = map(analogRead(THUMB_POT_1_IN),
                                    0, 1023, 255, 0);
}

// Used by patterns {2}
void update_blue_brightness() {
    current_rgbw[BLUE_INDEX] = map(analogRead(THUMB_POT_2_IN),
                                   0, 1023, 255, 0);
}

// TODO: this is temporary
void update_np_count() {
    if (NP_COUNT == 8) {
        NP_COUNT = 16;
    }
    else {
        NP_COUNT = 8;
        for (int i = NP_COUNT; i < ALL_NP_COUNT; i++) {
            pixels.setPixelColor(i, 0, 0, 0, 0);
        }
        pixels.show();
    }
}


/* ---------------------------------------------------------
 * OUTPUT UPDATE FUNCTIONS                                */

void show_pattern_and_free_mem() {
    int freemem = free_ram();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("pattern: ");
    lcd.print(current_pattern_fun_index);
    lcd.setCursor(0, 1);
    lcd.print("free mem: ");
    lcd.print(freemem);
    lcd.print("B");
    return;
}

void show_pattern_and_timing() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("pattern: ");
    lcd.print(current_pattern_fun_index);
    #if PROFILE
    lcd.setCursor(0, 1);
    lcd.print("loop: ");
    lcd.print(current_fun_inner_loop_time);
    #endif
    return;
}


/* ---------------------------------------------------------
 * SHARED FUNCTIONS                                       */

void write_RGBw_colors() {
    for (byte i = 0; i < NP_COUNT; i++) {
        if (gamma_correct_p) {
            pixels.setPixelColor(i,
              pgm_read_byte(&gamma_xlate[current_rgbw[RED_INDEX]]),
              pgm_read_byte(&gamma_xlate[current_rgbw[GREEN_INDEX]]),
              pgm_read_byte(&gamma_xlate[current_rgbw[BLUE_INDEX]]),
              pgm_read_byte(&gamma_xlate[current_rgbw[WHITE_INDEX]]));
        } else {
            pixels.setPixelColor(i, current_rgbw[RED_INDEX],
                                    current_rgbw[GREEN_INDEX],
                                    current_rgbw[BLUE_INDEX],
                                    current_rgbw[WHITE_INDEX]);
        }
    }
    pixels.show();
}

void write_RGBw_zeroes() {
    for (int i = 0; i < NP_COUNT; i++) {
        pixels.setPixelColor(i, 0, 0, 0, 0);
    }
    pixels.show();
}

// TODO: should I just remove this?
// TODO TODO TODO: don't use delay!!!
void do_strobe_delay() {
    return;
  /* // TODO: review this */
  /*   if (strobe_delay_0 > 27) { */
  /*       // TODO: fix */
  /*       write_RGBw_zeroes(); */
  /*       delay(strobe_delay_0); */
  /*   } */
}

bool update_all_devices() {
    if (re_timer > RE_CHECK_EVERY) {
        update_rotary_encoder();
        update_re_button();
        re_timer = 0;
    }
    if (pot_timer > POT_CHECK_EVERY) {
        update_thumb_pot_0();
        update_thumb_pot_1();
        update_thumb_pot_2();
        pot_timer = 0;
    }
    if (lcd_timer > LCD_EVERY) {
        update_LCD();
        lcd_timer = 0;
    }
    return !pattern_changed_p;
}


/* ---------------------------------------------------------
 * SHARED FUNCTIONS THAT LOOP                             */

// TODO TODO TODO: document
bool bring_down_color(byte color_index) {
    if (current_rgbw[color_index] >= STEP_DELTA) {
        if (step_timer > step_delay_0) {
            current_rgbw[color_index] = current_rgbw[color_index] - STEP_DELTA;
            write_RGBw_colors();
            // TODO TODO TODO: you again?
            /* do_strobe_delay(); */
            step_timer = 0;
        }
        return true;
    }
    return false;
}

bool bring_up_color(byte color_index) {
    if (current_rgbw[color_index] <= (255 - STEP_DELTA)) {
        if (step_timer > step_delay_0) {
            current_rgbw[color_index] = current_rgbw[color_index] + STEP_DELTA;
            write_RGBw_colors();
            // TODO TODO TODO: you again?
            /* do_strobe_delay(); */
            step_timer = 0;
        }
        return true;
    }
    return false;
}

// TODO TODO TODO: can I write `bring_color` that does both??



/* --------------------------------------------------------- */

/* PATTERN 0:
 *
 *   Shifts through all the colors. Speed, strobe, and brightness are
 *   parameterized.
 *
 */

void all_color_change_pattern_0() {

    #if DEBUG
    Serial.println("starting all_color_change_pattern_0");
    #endif

    /* ------- SETUP CODE ------- */
    update_thumb_pot_0  = update_brightness;
    update_thumb_pot_1  = update_step_delay;
    update_thumb_pot_2  = update_strobe_delay;
    sw_button_press     = update_np_count;
    update_LCD          = show_pattern_and_timing;

    /* ------- PATTERN LOOP ------- */
    while (!pattern_changed_p) {
        #if PROFILE
        inner_loop_time = 0;
        #endif

        // first sub-pattern
        while (update_all_devices() &&
               debug_values() &&
               bring_down_color(RED_INDEX)) {}
        while (update_all_devices() &&
               debug_values() &&
               bring_down_color(GREEN_INDEX)) {}
        while (update_all_devices() &&
               debug_values() &&
               bring_up_color(RED_INDEX)) {}
        while (update_all_devices() &&
               debug_values() &&
               bring_down_color(BLUE_INDEX)) {}
        while (update_all_devices() &&
               debug_values() &&
               bring_up_color(GREEN_INDEX)) {}
        while (update_all_devices() &&
               debug_values() &&
               bring_up_color(BLUE_INDEX)) {}

        // second sub-pattern
        while (update_all_devices() &&
               debug_values() &&
               bring_down_color(GREEN_INDEX)) {}
        while (update_all_devices() &&
               debug_values() &&
               bring_down_color(BLUE_INDEX)) {}
        while (update_all_devices() &&
               debug_values() &&
               bring_up_color(GREEN_INDEX)) {}
        while (update_all_devices() &&
               debug_values() &&
               bring_down_color(RED_INDEX)) {}
        while (update_all_devices() &&
               debug_values() &&
               bring_up_color(BLUE_INDEX)) {}
        while (update_all_devices() &&
               debug_values() &&
               bring_up_color(RED_INDEX)) {}

        // third sub-pattern
        while (update_all_devices() &&
               debug_values() &&
               bring_down_color(BLUE_INDEX)) {}
        while (update_all_devices() &&
               debug_values() &&
               bring_down_color(RED_INDEX)) {}
        while (update_all_devices() &&
               debug_values() &&
               bring_up_color(BLUE_INDEX)) {}
        while (update_all_devices() &&
               debug_values() &&
               bring_down_color(GREEN_INDEX)) {}
        while (update_all_devices() &&
               debug_values() &&
               bring_up_color(RED_INDEX)) {}
        while (update_all_devices() &&
               debug_values() &&
               bring_up_color(GREEN_INDEX)) {}

        #if PROFILE
        current_fun_inner_loop_time = inner_loop_time;
        #endif
    }

    /* ------- TEARDOWN CODE ------- */
    #if DEBUG
    Serial.println("ending all_color_change_pattern_0");
    #endif
}

/* --------------------------------------------------------- */

/* PATTERN 1:
 *
 *   Shifts through all the colors. Speed, strobe, and brightness are
 *   parameterized.
 *
 *   Avoids making white
 *
 *   Uses gamma correction by default
 *
 */

void all_color_change_pattern_1() {

    #if DEBUG
    Serial.println("starting all_color_change_pattern_1");
    #endif

    /* ------- SETUP CODE ------- */
    bring_down_color(RED_INDEX); // starts at 011
    current_rgbw[RED_INDEX]   = 0;
    current_rgbw[GREEN_INDEX] = 255;
    current_rgbw[BLUE_INDEX]  = 255;

    update_thumb_pot_0  = update_brightness;
    update_thumb_pot_1  = update_step_delay;
    update_thumb_pot_2  = update_strobe_delay;
    sw_button_press     = update_np_count;
    update_LCD          = show_pattern_and_timing;

    gamma_correct_p = true;

    step_timer = 0;

    /* ------- PATTERN LOOP ------- */
    while (!pattern_changed_p){
        #if PROFILE
        inner_loop_time = 0;
        #endif

        while (update_all_devices() &&
               debug_values() &&
               bring_down_color(GREEN_INDEX)) {}
        while (update_all_devices() &&
               debug_values() &&
               bring_up_color(RED_INDEX))     {}
        while (update_all_devices() &&
               debug_values() &&
               bring_down_color(BLUE_INDEX))  {}
        while (update_all_devices() &&
               debug_values() &&
               bring_up_color(GREEN_INDEX))   {}
        while (update_all_devices() &&
               debug_values() &&
               bring_down_color(RED_INDEX))   {}
        while (update_all_devices() &&
               debug_values() &&
               bring_up_color(BLUE_INDEX))    {}

        #if PROFILE
        current_fun_inner_loop_time = inner_loop_time;
        #endif
    }

    /* ------- TEARDOWN CODE ------- */
    #if DEBUG
    Serial.println("ending all_color_change_pattern_1");
    #endif
}

/* --------------------------------------------------------- */

/* PATTERN 2:
 *
 *   Solid color with option to change the values for each RGB channel
 *
 */

void solid_color_pattern() {

    #if DEBUG
    Serial.println("starting solid_color_pattern");
    #endif

    /* ------- SETUP CODE ------- */
    update_thumb_pot_0  = update_red_brightness;
    update_thumb_pot_1  = update_green_brightness;
    update_thumb_pot_2  = update_blue_brightness;
    sw_button_press     = update_np_count;
    update_LCD          = show_pattern_and_timing;

    brightness = 255;
    update_brightness();

    write_RGBw_colors();

    /* ------- PATTERN LOOP ------- */
    while (!pattern_changed_p) {
        #if PROFILE
        inner_loop_time = 0;
        #endif

        update_all_devices();
        debug_values();
        // stop the step timer from overflowing
        step_timer = 0;
        write_RGBw_colors();

        #if PROFILE
        current_fun_inner_loop_time = inner_loop_time;
        #endif
    }

    /* ------- TEARDOWN CODE ------- */
    #if DEBUG
    Serial.println("ending solid_color_pattern");
    #endif
}


/* --------------------------------------------------------- */

/* PATTERN 3:
 *
 *   Warm light with controllable brightness (but that's it)
 *
 */

void warm_light_pattern() {

    #if DEBUG
    Serial.println("starting warm_light_pattern");
    #endif

    /* ------- SETUP CODE ------- */
    update_thumb_pot_0  = update_brightness;
    update_thumb_pot_1  = nothing_function;
    update_thumb_pot_2  = nothing_function;
    sw_button_press     = update_np_count;
    update_LCD          = show_pattern_and_timing;

    current_rgbw[RED_INDEX]   = 0;
    current_rgbw[GREEN_INDEX] = 0;
    current_rgbw[BLUE_INDEX]  = 0;
    current_rgbw[WHITE_INDEX] = 255;

    write_RGBw_colors();

    /* ------- PATTERN LOOP ------- */
    while (!pattern_changed_p) {
        #if PROFILE
        inner_loop_time = 0;
        #endif

        update_all_devices();
        debug_values();
        // stop the step timer from overflowing
        step_timer = 0;
        write_RGBw_colors();

        #if PROFILE
        current_fun_inner_loop_time = inner_loop_time;
        #endif
    }

    /* ------- TEARDOWN CODE ------- */
    current_rgbw[WHITE_INDEX] = 0;

    #if DEBUG
    Serial.println("ending warm_light_pattern");
    #endif
}


/* ---------------------------------------------------------
 * PATTERN FUNCTION ARRAY                                 */

PatternFunction pattern_functions[NUM_PATTERNS] = {
    all_color_change_pattern_0,
    all_color_change_pattern_1,
    solid_color_pattern,
    warm_light_pattern
};


/* ---------------------------------------------------------
 * SETUP AND MAINLOOP                                     */

void setup() {
  	pinMode(RE_CLK,        INPUT);
    pinMode(RE_DT_LAG,     INPUT);
    pinMode(RE_SW_BUTTON,  INPUT_PULLUP);

    pinMode(PHOTORESISTOR, INPUT);

    update_brightness();
    pixels.begin();
    pixels.setBrightness(brightness);

    #if DEBUG
    Serial.begin(115200);
    Serial.println("started serial");
    #endif

    sbi(ADCSRA, ADPS2);
    cbi(ADCSRA, ADPS1);
    cbi(ADCSRA, ADPS0);


    lcd.init();
    lcd.backlight();
    lcd.setCursor(3, 0);
    lcd.print("Light Box");
    lcd.setCursor(2, 1);
    lcd.print("-------------");

}


void loop() {

    /*
      Each pattern has it's own main loop that will only exit if
      a pattern change is detected (e.g. through the rotary encoder.
      At that time, control is returned to this loop to start the
      process over again with the correct pattern
                                                                      */
    pattern_changed_p = false;
    pattern_functions[current_pattern_fun_index]();

}



/* ---------------------------------------------------------
 * PROGMEM things                                         */

// uses a gamma of 2.8 (largely chosen arbitrarily)
const byte PROGMEM gamma_xlate[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 };



