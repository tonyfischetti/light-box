
/************************************************************************
 *                                                                      *
 *  light-box.c                                                         *
 *                                                                      *
 *    author: tony fischetti <tony.fischetti@gmail.com>                 *
 *    license: GPL-3                                                    *
 *                                                                      *
 *    ---------------                                                   *
 *                                                                      *
 *    There was once a young man who was very sad. He always stayed     *
 *    in the dark. Then, one day, he wasn't so sad anymore and          *
 *    learned that he loves light.                                      *
 *                                                                      *
 *    This code goes out to that young man.                             *
 *                                                                      *
 *                                                                      *
 ************************************************************************/


#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <elapsedMillis.h>

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <IRremote.hpp>



/********************************************************
 * SOME TODOS                                           *
 *                                                      *
 *   - Experiment with `MAX_BRIGHTNESS[4]`              *
 *     especially for green                             *
 *   - Test with different optimization levels          *
 *   - Consider a struct holding pattern info like      *
 *     pattern name, function, and function overrides   *
 *     That way, the LCD display can show what changed  *
 *   - Actually, instead of a struct, consider a class  *
 *     https://cplusplus.com/doc/tutorial/classes/      *
 *   - Use different DEBUG levels                       *
 *     (profile these separately)                       *
 *   - Fix bug regarding LCD not turning backlight      *
 *     back on if control is already away from the IR   *
 *   - Have a pattern with "uniform" brightness         *
 *   - Use `static` where I can                         *
 *   - Use `const` where I can                          *
 *   - Use unsigned data types where I can              *
 *   - Test different gamma values                      *
 *   - Have another pattern (like 1) but avoids         *
 *     spending too much time on chartreuse             *
 *                                                      *
 ********************************************************/


/* ---------------------------------------------------------
 * PROGMEM (so we can put it at the bottom)               */

extern const byte gamma_xlate[];



/* ---------------------------------------------------------
 * MACROS                                                 */

#define DEBUG   false
#define PROFILE true

#define PRO_MICRO true
#define ARD_UNO false

// Total number of available Neopixel (even if they're not all used)
#define ALL_NP_COUNT 24

// number of milliseconds to wait for buttons, etc. to settle
#define EPSILON 250

// amount the thumb pots must change to steal control back
// from the IR remote
#define ANALOG_DEV_TOLERANCE 2

#define LCD_TIMEOUT 20000

// The indices into the `current_rgbw` array for each color
#define RED_INDEX   0
#define GREEN_INDEX 1
#define BLUE_INDEX  2
#define WHITE_INDEX 3

#define DBUG_EVERY      3000
#define POT_CHECK_EVERY 50
#define RE_CHECK_EVERY  10
#define LCD_EVERY       500
#define IR_CHECK_EVERY  20

// ACHTUNG: NtS: update everytime I add a pattern
#define NUM_PATTERNS 4

// fix
#define UP        true
#define DOWN      false
#define INCREMENT true
#define DECREMENT false



/* ---------------------------------------------------------
 * PIN MACROS                                             */

/* - - FOR PRO MICRO - - - - - - -*/
#if PRO_MICRO
#define SDA             2   // orange
#define SCL             3   // periwinkle
#define BUZZER          4   // dark pink
#define NEOPIXEL_PIN    5   // sea green
#define PHOTORESISTOR   A7  // (6) yellow
#define RE_SW_BUTTON    7   // navy
#define RE_DT_LAG       8   // purple
#define RE_CLK          9   // green
#define IR_PIN          10  // light pink
#define THUMB_POT_0_IN  A0
#define THUMB_POT_1_IN  A1
#define THUMB_POT_2_IN  A2
#endif


/* - - FOR ARDUINO UNO - - - - - - -*/
#if ARD_UNO
#define SDA             A4
#define SCL             A5   // periwinkle
/* #define BUZZER */
#define NEOPIXEL_PIN    7
/* #define PHOTORESISTOR */
#define RE_SW_BUTTON    11
#define RE_DT_LAG       12
#define RE_CLK
/* #define IR_PIN */
#define THUMB_POT_0_IN  A0
#define THUMB_POT_1_IN  A1
#define THUMB_POT_2_IN  A2
#endif




/* ---------------------------------------------------------
 * IR REMOTE MACROS                                       */

#define REM_POWER   69
#define REM_VOL_UP  70
#define REM_FUNC    71
#define REM_BACK    68
#define REM_FORWARD 67
#define REM_DOWN    7
#define REM_VOL_DWN 21
#define REM_UP      9
#define REM_EQ      25
#define REM_ST      13
#define REM_ZERO    22
#define REM_ONE     12
#define REM_TWO     24
#define REM_THREE   94
/* #define REM_FOUR    8 */
/* #define REM_FIVE    28 */
/* #define REM_SIX     90 */
// TODO TODO TODO: temporary
#define REM_SEVEN   66
#define REM_EIGHT   82
#define REM_NINE    74
/* #define REM_PLAY    64 */



/* ---------------------------------------------------------
 * GLOBALS                                                */

// holds the NeoPixel object
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(ALL_NP_COUNT, NEOPIXEL_PIN,
                                             NEO_GRBW + NEO_KHZ800);

// holds the values for each RGBw channel (globally)
byte current_rgbw[4] = {255, 255, 255, 0};

// holds the maximum brightness each channel should reach
byte max_brightnesses[4] = {255, 255, 255, 255};

// flag that gets set when the pattern is changed
bool pattern_changed_p = false;

#if PROFILE
unsigned long current_fun_inner_loop_time = 0;
#endif

// timers for various things that need timers
elapsedMillis debug_timer;
elapsedMillis pot_timer;
elapsedMillis re_timer;
elapsedMillis step_timer;
elapsedMillis lcd_timer;
elapsedMillis lcd_timeout;
elapsedMillis ir_timer;

#if PROFILE
elapsedMillis inner_loop_time;
#endif

// flag indicating whether it is the IR remote
// or the on-box controls that are in command
bool control_to_ir = false;

LiquidCrystal_I2C lcd(0x27, 20, 4);



/* ---------------------------------------------------------
 * SENSOR VALUE VARIABLES                                 */

// holds the index (into pattern_functions) for the current
// pattern's function pointer
// Used by all patterns (rotary encoder and IR)
byte current_pattern_fun_index = 1;

// used by patterns {0, 1, 2} (potentiometer)
byte brightness = 255;

// used by patterns {0, 1} (potentiometer)
byte step_delay = 1;

// value to change the color by on each step
// used by patterns {0, 1} (potentiometer)
byte step_delta = 2;

// Number of NeoPixel LEDS to use
// Used by all patterns (rotary button and IR)
byte np_count = 8;

// Used by all patterns (IR)
bool gamma_correct_p = true;



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
        Serial.print(F("Pattern:    "));
        Serial.println(current_pattern_fun_index);
        Serial.print(F("RGBW:       "));
        Serial.print(current_rgbw[RED_INDEX]);
        Serial.print(F(", "));
        Serial.print(current_rgbw[GREEN_INDEX]);
        Serial.print(F(", "));
        Serial.print(current_rgbw[BLUE_INDEX]);
        Serial.print(F(", "));
        Serial.println(current_rgbw[WHITE_INDEX]);
        Serial.print(F("brightness: "));
        Serial.println(brightness);
        Serial.flush();
        Serial.print(F("gamma:      "));
        Serial.println(gamma_correct_p);
        Serial.print(F("step_timer:  "));
        Serial.println(step_timer);
        Serial.print(F("debug timer: "));
        Serial.println(debug_timer);
        Serial.print(F("pot timer:   "));
        Serial.println(pot_timer);
        Serial.print(F("re_timer:    "));
        Serial.println(re_timer);
        Serial.print(F("step delay: "));
        Serial.println(step_delay);
        Serial.print(F("step delta: "));
        Serial.println(step_delta);
        Serial.println("-----------------");
        Serial.flush();
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
 * HELPER FUNCTIONS                                        */

// NOTE: has side-effects of mutating `control_to_ir` and
// resetting lcd_timeout
bool analog_changed_sufficiently_p(byte previous, byte current) {
    if (abs(current - previous) > ANALOG_DEV_TOLERANCE) {
        if (control_to_ir) {
            #if DEBUG
            Serial.println(F("taking back control from IR"));
            #endif
            control_to_ir = false;
        }
        lcd_timeout = 0;
        return true;
    }
    return false;
}

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
    lcd_timeout = 0;
}

// Used by all patterns
// Changes the number of LED stick that'll light up
void update_np_count() {
    // TODO: make bi-directional
    if (np_count == 8) {
        np_count = 16;
    }
    else if (np_count == 16) {
        np_count = 24;
    }
    else {
        np_count = 8;
        for (int i = np_count; i < ALL_NP_COUNT; i++) {
            pixels.setPixelColor(i, 0, 0, 0, 0);
        }
        while (!IrReceiver.isIdle()) { }
        pixels.show();
    }
    lcd_timeout = 0;
}



/* ---------------------------------------------------------
 * SENSOR UPDATE FUNCTIONS                                 */

    /******* DIGITAL *******/

// This one stays the same for all patterns
bool update_rotary_encoder() {
    bool current_state_CLK = digitalRead(RE_CLK);
    static bool previous_state_CLK = current_state_CLK;
    bool current_state_DT;

    current_state_CLK = digitalRead(RE_CLK);
    if (current_state_CLK != previous_state_CLK  && current_state_CLK == 1){
        current_state_DT = digitalRead(RE_DT_LAG);
        if (current_state_DT != current_state_CLK) {
            #if DEBUG
            Serial.println(F("supposed to increment"));
            #endif
            update_current_pattern_fun_index(true);
        } else {
            #if DEBUG
            Serial.println(F("supposed to decrement"));
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
    static unsigned long previous_sw_button_press;
    unsigned long current_millis = millis();
    if (digitalRead(RE_SW_BUTTON)==LOW) {
        if ((current_millis - previous_sw_button_press) > EPSILON){
            #if DEBUG
            Serial.println(F("button clicked. about to run `sw_button_press`"));
            #endif
            lcd_timeout = 0;
            sw_button_press();
        }
        previous_sw_button_press = current_millis;
    }
}

// TODO TODO TODO: document
void update_ir() {
    static unsigned long previous_ir_signal;
    byte command;
    unsigned long current_millis = millis();
    if (IrReceiver.decode()) {
        IrReceiver.resume();
        // will turn on LCD backlight for any IR signal. want.
        lcd_timeout = 0;
        // TODO TODO TODO: should I check if it's the right remote??
        command = IrReceiver.decodedIRData.command;
        if (!control_to_ir) {
            #if DEBUG
            Serial.println(F("giving control to IR"));
            #endif
            control_to_ir = true;
        }
        if ((current_millis - previous_ir_signal) > EPSILON) {
            #if DEBUG
            Serial.print(F("command was: "));
            Serial.println(command);
            #endif
            switch (command) {
                case REM_POWER:
                    update_np_count();
                    break;
                case REM_VOL_UP:
                    brightness = constrain(brightness + 25, 0, 255);
                    pixels.setBrightness(brightness);
                    break;
                case REM_VOL_DWN:
                    brightness = constrain(brightness - 25, 0, 255);
                    pixels.setBrightness(brightness);
                    break;
                case REM_BACK:
                    update_current_pattern_fun_index(false);
                    break;
                case REM_FORWARD:
                    update_current_pattern_fun_index(true);
                    break;
                case REM_FUNC:
                    gamma_correct_p = !gamma_correct_p;
                    break;

                case REM_UP:
                    Serial.println("increasing step delay");
                    step_delay = constrain(step_delay + 5, 1, 255);
                    break;
                case REM_DOWN:
                    Serial.println("Decreasing step delay");
                    step_delay = constrain(step_delay - 5, 1, 255);
                    break;
                case REM_ST:
                    Serial.println("increasing step delta");
                    step_delta = constrain(step_delta + 1, 1, 50);
                    break;
                case REM_EQ:
                    Serial.println("Decreasing step delta");
                    step_delta = constrain(step_delta - 1, 1, 50);
                    break;

                case REM_ZERO:
                    current_pattern_fun_index = 0;
                    pattern_changed_p = true;
                    break;
                case REM_ONE:
                    current_pattern_fun_index = 1;
                    pattern_changed_p = true;
                    break;
                case REM_TWO:
                    current_pattern_fun_index = 2;
                    pattern_changed_p = true;
                    break;
                case REM_THREE:
                    current_pattern_fun_index = 3;
                    pattern_changed_p = true;
                    break;
                    // ACHTUNG: NtS: update

                // TODO TODO TODO: temporary
                case REM_SEVEN:
                    update_LCD = show_free_mem_and_timing;
                    break;
                case REM_EIGHT:
                    update_LCD = show_ir_and_brightness;
                    break;
                case REM_NINE:
                    update_LCD = show_step_info;
                    break;

                default:
                    update_LCD = show_home;
            }
        }
        previous_ir_signal = current_millis;
    }
}

    /******* ANALOG *******/

// Used by patterns {0, 1, 2, 4} (NtS)
void update_brightness() {
    /* I used and `int` here (and the others for a "good" reason). When light
    box turns on for the first time, the brightness (or other) from the
    knobs' position won't get set if the knob's position happens to be
    whatever I arbitrarily set `previous_brightness` to. While it's
    unlikely, on any one trial, that the two would be within 2 of eachother,
    over the long run, it's unlikely _not_ to happen.
    By setting `previous_brightness` negative, I can ensure that the knob's
    position is _always_ sufficiently different than the previous value     */
    static int previous_brightness = -100;
    byte current_brightness = map(analogRead(THUMB_POT_0_IN), 0, 1023, 255, 1);
    if (analog_changed_sufficiently_p(previous_brightness,
                current_brightness)) {
        previous_brightness = current_brightness;
        brightness = current_brightness;
        pixels.setBrightness(brightness);
    }
}

// Used by patterns {0, 1} (NtS)
void update_step_delay() {
    static int previous_step_delay = -100;
    byte current_step_delay = map(analogRead(THUMB_POT_1_IN), 0, 1023, 255, 1);
    if (analog_changed_sufficiently_p(previous_step_delay,
                current_step_delay)) {
        previous_step_delay = current_step_delay;
        step_delay = current_step_delay;
    }
}

// Used by patterns {0, 1} (NtS)
void update_step_delta() {
    // TODO TODO: are these appropriate limits?
    static int previous_step_delta = - 100;
    byte current_step_delta = map(analogRead(THUMB_POT_2_IN), 0, 1023, 100, 1);
    if (analog_changed_sufficiently_p(previous_step_delta,
                current_step_delta)) {
        previous_step_delta = current_step_delta;
        step_delta = current_step_delta;
    }
}

// Used by patterns {2} (NtS)
void update_red_brightness() {
    static int previous_red = -100;
    byte current_red = map(analogRead(THUMB_POT_0_IN),
                           0, 1023, max_brightnesses[RED_INDEX], 0);
    if (analog_changed_sufficiently_p(previous_red, current_red)) {
        previous_red = current_red;
        current_rgbw[RED_INDEX] = current_red;
    }
}

// Used by patterns {2} (NtS)
void update_green_brightness() {
    static int previous_green = -100;
    byte current_green = map(analogRead(THUMB_POT_1_IN),
                             0, 1023, max_brightnesses[GREEN_INDEX], 0);
    if (analog_changed_sufficiently_p(previous_green, current_green)) {
        previous_green = current_green;
        current_rgbw[GREEN_INDEX] = current_green;
    }
}

// Used by patterns {2} (NtS)
void update_blue_brightness() {
    static int previous_blue = -100;
    byte current_blue = map(analogRead(THUMB_POT_2_IN),
                            0, 1023, max_brightnesses[BLUE_INDEX], 0);
    if (analog_changed_sufficiently_p(previous_blue, current_blue)) {
        previous_blue = current_blue;
        current_rgbw[BLUE_INDEX] = current_blue;
    }
}



/* ---------------------------------------------------------
 * OUTPUT UPDATE FUNCTIONS                                */

// TODO TODO TODO: this whole thing smacks of gender

void show_home() {
    lcd.clear();
    lcd.setCursor(3, 0);
    lcd.print("Light Box");
    lcd.setCursor(2, 1);
    lcd.print("-------------");
}

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
}

void show_free_mem_and_timing() {
    int freemem = free_ram();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("free mem: ");
    lcd.print(freemem);
    lcd.print("B");
    #if PROFILE
    lcd.setCursor(0, 1);
    lcd.print("loop: ");
    lcd.print(current_fun_inner_loop_time);
    #endif
}

void show_ir_and_brightness() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ir control: ");
    lcd.print(control_to_ir);
    lcd.setCursor(0, 1);
    lcd.print("brightness: ");
    lcd.print(brightness);
}

void show_rgb_and_gamma() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("<");
    lcd.print(current_rgbw[RED_INDEX]);
    lcd.print(", ");
    lcd.print(current_rgbw[GREEN_INDEX]);
    lcd.print(", ");
    lcd.print(current_rgbw[BLUE_INDEX]);
    lcd.print(">");
    lcd.setCursor(0, 1);
    lcd.print("gamma: ");
    lcd.print(gamma_correct_p);
}

void show_step_info() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("step_delay: ");
    lcd.print(step_delay);
    lcd.setCursor(0, 1);
    lcd.print("step delta: ");
    lcd.print(step_delta);
}


/* ---------------------------------------------------------
 * SHARED FUNCTIONS                                       */

void write_RGBw_colors() {
    for (byte i = 0; i < np_count; i++) {
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
    while (!IrReceiver.isIdle()) { }
    pixels.show();
}

void write_RGBw_zeroes() {
    for (int i = 0; i < np_count; i++) {
        pixels.setPixelColor(i, 0, 0, 0, 0);
    }
    while (!IrReceiver.isIdle()) { }
    pixels.show();
}

bool update_all_devices() {
    if (ir_timer > IR_CHECK_EVERY) {
        update_ir();
        ir_timer = 0;
    }
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
        if (lcd_timeout >= LCD_TIMEOUT) {
            lcd.noBacklight();
        } else {
            lcd.backlight();
            update_LCD();
        }
        lcd_timer = 0;
    }
    return !pattern_changed_p;
}



/* ---------------------------------------------------------
 * SHARED FUNCTIONS FOR COLOR SHIFTING                    */

bool room_to_go_p(bool direction, byte color_index) {
    // UP
    if (direction)
        return (current_rgbw[color_index] <=
                  (max_brightnesses[color_index] - step_delta));
    else
        return (current_rgbw[color_index] >= step_delta);
}

bool move_color(bool direction, byte color_index, bool reset_timer_p=true) {
    if (step_timer > step_delay) {
        if (room_to_go_p(direction, color_index)) {
            // UP
            if (direction)
                current_rgbw[color_index] = current_rgbw[color_index] +
                                              step_delta;
            else
                current_rgbw[color_index] = current_rgbw[color_index] -
                                              step_delta;
            write_RGBw_colors();
            if (reset_timer_p)
                step_timer = 0;
            return true;
        }
        else
            return false;
    }
    // we have to assume there's more to go
    return true;
}

bool move_colors(bool direction, byte n_colors, byte* color_indices) {
    bool continue_p = false;
    if (step_timer > step_delay) {
        for (int i = 0; i < n_colors; i++) {
            byte current_color = color_indices[i];
            if (move_color(direction, current_color, false))
                continue_p = true;
        }
        step_timer = 0;
        return continue_p;
    }
    return true;
}



/* --------------------------------------------------------- */

/* PATTERN 0:
 *
 *   Shifts through all the colors. Brightness, speed, and step size are
 *   parameterized.
 *
 */

void all_color_change_pattern_0() {

    #if DEBUG
    Serial.println(F("starting all_color_change_pattern_0"));
    #endif

    /* ------- SETUP CODE ------- */
    update_thumb_pot_0  = update_brightness;
    update_thumb_pot_1  = update_step_delay;
    update_thumb_pot_2  = update_step_delta;
    sw_button_press     = update_np_count;
    update_LCD          = show_home;

    /* ------- PATTERN LOOP ------- */
    while (!pattern_changed_p) {
        #if PROFILE
        inner_loop_time = 0;
        #endif

        // first sub-pattern
        while (update_all_devices() &&
                debug_values() &&
                move_color(DOWN, RED_INDEX))   {}
        while (update_all_devices() &&
                debug_values() &&
                move_color(DOWN, GREEN_INDEX)) {}
        while (update_all_devices() &&
                debug_values() &&
                move_color(UP, RED_INDEX))     {}
        while (update_all_devices() &&
                debug_values() &&
                move_color(DOWN, BLUE_INDEX))  {}
        while (update_all_devices() &&
                debug_values() &&
                move_color(UP, GREEN_INDEX))   {}
        while (update_all_devices() &&
                debug_values() &&
                move_color(UP, BLUE_INDEX))    {}

        // second sub-pattern
        while (update_all_devices() &&
                debug_values() &&
                move_color(DOWN, GREEN_INDEX)) {}
        while (update_all_devices() &&
                debug_values() &&
                move_color(DOWN, BLUE_INDEX))  {}
        while (update_all_devices() &&
                debug_values() &&
                move_color(UP, GREEN_INDEX))   {}
        while (update_all_devices() &&
                debug_values() &&
                move_color(DOWN, RED_INDEX))   {}
        while (update_all_devices() &&
                debug_values() &&
                move_color(UP, BLUE_INDEX))    {}
        while (update_all_devices() &&
                debug_values() &&
                move_color(UP, RED_INDEX))     {}

        // third sub-pattern
        while (update_all_devices() &&
                debug_values() &&
                move_color(DOWN, BLUE_INDEX))  {}
        while (update_all_devices() &&
                debug_values() &&
                move_color(DOWN, RED_INDEX))   {}
        while (update_all_devices() &&
                debug_values() &&
                move_color(UP, BLUE_INDEX))    {}
        while (update_all_devices() &&
                debug_values() &&
                move_color(DOWN, GREEN_INDEX)) {}
        while (update_all_devices() &&
                debug_values() &&
                move_color(UP, RED_INDEX))     {}
        while (update_all_devices() &&
                debug_values() &&
                move_color(UP, GREEN_INDEX))   {}

        #if PROFILE
        current_fun_inner_loop_time = inner_loop_time;
        #endif
    }

    /* ------- TEARDOWN CODE ------- */
    #if DEBUG
    Serial.println(F("ending all_color_change_pattern_0"));
    #endif
}


/* --------------------------------------------------------- */

/* PATTERN 1:
 *
 *   Shifts through all the colors. Brightness, speed, and step size are
 *   parameterized.
 *
 *   Avoids making white
 *
 */

void all_color_change_pattern_1() {

    #if DEBUG
    Serial.println(F("starting all_color_change_pattern_1"));
    #endif

    /* ------- SETUP CODE ------- */
    move_color(DOWN, RED_INDEX); // starts at 011
    current_rgbw[RED_INDEX]   = 0;
    current_rgbw[GREEN_INDEX] = 255;
    current_rgbw[BLUE_INDEX]  = 255;

    update_thumb_pot_0  = update_brightness;
    update_thumb_pot_1  = update_step_delay;
    update_thumb_pot_2  = update_step_delta;
    sw_button_press     = update_np_count;
    update_LCD          = show_home;

    gamma_correct_p = true;

    step_timer = 0;

    /* ------- PATTERN LOOP ------- */
    while (!pattern_changed_p){
        #if PROFILE
        inner_loop_time = 0;
        #endif

        while (update_all_devices() &&
                debug_values() &&
                move_color(DOWN, GREEN_INDEX)) {}
        while (update_all_devices() &&
                debug_values() &&
                move_color(UP, RED_INDEX))     {}
        while (update_all_devices() &&
                debug_values() &&
                move_color(DOWN, BLUE_INDEX))  {}
        while (update_all_devices() &&
                debug_values() &&
                move_color(UP, GREEN_INDEX))   {}
        while (update_all_devices() &&
                debug_values() &&
                move_color(DOWN, RED_INDEX))   {}
        while (update_all_devices() &&
                debug_values() &&
                move_color(UP, BLUE_INDEX))    {}

        #if PROFILE
        current_fun_inner_loop_time = inner_loop_time;
        #endif
    }

    /* ------- TEARDOWN CODE ------- */
    #if DEBUG
    Serial.println(F("ending all_color_change_pattern_1"));
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
    Serial.println(F("starting solid_color_pattern"));
    #endif

    /* ------- SETUP CODE ------- */
    update_thumb_pot_0  = update_red_brightness;
    update_thumb_pot_1  = update_green_brightness;
    update_thumb_pot_2  = update_blue_brightness;
    sw_button_press     = update_np_count;
    update_LCD          = show_rgb_and_gamma;

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
        // TODO TODO TODO TODO: check it see if they changed first
        write_RGBw_colors();

        #if PROFILE
        current_fun_inner_loop_time = inner_loop_time;
        #endif
    }

    /* ------- TEARDOWN CODE ------- */
    #if DEBUG
    Serial.println(F("ending solid_color_pattern"));
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
    Serial.println(F("starting warm_light_pattern"));
    #endif

    /* ------- SETUP CODE ------- */
    update_thumb_pot_0  = update_brightness;
    update_thumb_pot_1  = nothing_function;
    update_thumb_pot_2  = nothing_function;
    sw_button_press     = update_np_count;
    update_LCD          = show_home;

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
        // TODO TODO TODO TODO: check it see if they changed first
        write_RGBw_colors();

        #if PROFILE
        current_fun_inner_loop_time = inner_loop_time;
        #endif
    }

    /* ------- TEARDOWN CODE ------- */
    current_rgbw[WHITE_INDEX] = 0;

    #if DEBUG
    Serial.println(F("ending warm_light_pattern"));
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
    Serial.println(F("started serial"));
    #endif

    IrReceiver.begin(IR_PIN);

    lcd.init();
    lcd.backlight();
    show_home();

}


void loop() {

    /*
      Each pattern has it's own main loop that will only exit if
      a pattern change is detected (i.e. through the rotary encoder
      or IR remote). At that time, control is returned to this loop
      to start the process over again with the correct pattern
                                                                      */
    pattern_changed_p = false;
    pattern_functions[current_pattern_fun_index]();

}



/* ---------------------------------------------------------
 * PROGMEM things                                         */

/*
// gamma: 2.8
const byte PROGMEM gamma_xlate[] = {
0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9,  10,
10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
90, 92, 93, 95, 96, 98, 99, 101,102,104,105,107,109,110,112,114,
115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255
};
 */

// gamma: 4
const byte PROGMEM gamma_xlate[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9,
    9,  10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16,
    16, 17, 17, 18, 18, 19, 19, 20, 21, 21, 22, 23, 23, 24, 25, 25,
    26, 27, 27, 28, 29, 30, 31, 31, 32, 33, 34, 35, 36, 37, 38, 39,
    40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 52, 53, 54, 55, 57,
    58, 59, 61, 62, 63, 65, 66, 68, 69, 71, 72, 74, 75, 77, 79, 80,
    82, 84, 85, 87, 89, 91, 93, 95, 96, 98, 100,102,104,107,109,111,
    113,115,117,120,122,124,126,129,131,134,136,139,141,144,146,149,
    152,155,157,160,163,166,169,172,175,178,181,184,187,190,194,197,
    200,203,207,210,214,217,221,224,228,232,236,239,243,247,251,255
};


