
/************************************************************************
 *                                                                      *
 *  light-box.c                                                         *
 *                                                                      *
 *    author: tony fischetti <tony.fischetti@gmail.com>                 *
 *    license: GPL-3                                                    *
 *                                                                      *
 *                - - - - - - - - - - - - - - - - - -                   *
 *                                                                      *
 *    There was once a young man who was very sad. He always stayed     *
 *    in the dark. Then, one day, he wasn't so sad anymore and          *
 *    learned that he loves light.                                      *
 *                                                                      *
 *    This code goes out to that young man.                             *
 *                                                                      *
 *                                                                      *
 ************************************************************************/

#include <stdint.h>

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <elapsedMillis.h>
#include <LiquidCrystal_I2C.h>
#include <IRremote.hpp>



/********************************************************
 * SOME TODOS                                           *
 *                                                      *
 *                                                      *
 *   - Detect if LCD changes... if it doesn't,          *
 *     no need to update it                             *
 *   - Skip chartreuse by going forward to _______,     *
 *     stopping, and then going back                    *
 *   - Try taking DEBUG (and PROFILE?) out of the this  *
 *     file and adding to the Makefile (eg. -DDEBUG,    *
 *     -DPROFILE)                                       *
 *     ! The `#if`s have to be changed to `#ifdef`s,    *
 *     in that case                                     *
 *   - Does the new remote send a different code when   *
 *     you press and _hold down_ button?                *
 *   - Experiment with `max_brightnesses`               *
 *     especially for green                             *
 *   - Test with different optimization levels          *
 *   - Use different DEBUG levels                       *
 *     (profile these separately)                       *
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

// Total number of available Neopixel (even if they're not all used)
#define ALL_NP_COUNT 32

// number of milliseconds to wait for buttons, etc. to settle
#define EPSILON 125

// amount the thumb pots must change to steal control back
// from the IR remote
#define ANALOG_DEV_TOLERANCE 4

// The indices into the `current_rgbw` array for each color
#define RED_INDEX   0
#define GREEN_INDEX 1
#define BLUE_INDEX  2
#define WHITE_INDEX 3

#define DBUG_EVERY      3000
#define POT_CHECK_EVERY 50
#define RE_CHECK_EVERY  10
#define LCD_EVERY       100
#define IR_CHECK_EVERY  20

#define LCD_TIMEOUT 10000

// TODO TODO TODO TODO: document
//                      has to change with LCD_EVERY
#define LCD_OVERRIDE_LENGTH 30

// TODO TODO TODO TODO: document
#define STEP_DIVISOR 4

// ACHTUNG: NtS: update everytime I add a pattern
#define NUM_PATTERNS 6
// fix
#define UP        true
#define DOWN      false
#define INCREMENT true
#define DECREMENT false



/* ---------------------------------------------------------
 * PIN MACROS                                             */

/* - - FOR PRO MICRO - - - - - - -*/
#ifdef ARDUINO_AVR_LEONARDO
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
#ifdef ARDUINO_AVR_UNO
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
// TODO TODO TODO: temporary
#define REM_FOUR    8
#define REM_FIVE    28
#define REM_SIX     90
#define REM_SEVEN   66
#define REM_EIGHT   82
#define REM_NINE    74
/* #define REM_PLAY    64 */



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

typedef void (*RemoteFunction) ();
RemoteFunction rem_vol_up   = nothing_function;
RemoteFunction rem_vol_down = nothing_function;
RemoteFunction rem_up       = nothing_function;
RemoteFunction rem_down     = nothing_function;
RemoteFunction rem_st       = nothing_function;
RemoteFunction rem_eq       = nothing_function;



/* ---------------------------------------------------------
 * OTHER TYPEDEFS                                          */

typedef uint8_t byte;



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
uint32_t current_fun_inner_loop_time = 0;
#endif

// timers for various things that need timers
elapsedMillis debug_timer;
elapsedMillis pot_timer;
elapsedMillis re_timer;
elapsedMillis step_timer;
elapsedMillis on_timer;
elapsedMillis off_timer;
elapsedMillis lcd_timer;
elapsedMillis lcd_timeout;
elapsedMillis ir_timer;

#if PROFILE
elapsedMillis inner_loop_time;
#endif

// flag indicating whether it is the IR remote
// or the on-box controls that are in command
bool control_to_ir = false;

// when we start a new pattern, etc... we need to
//force update sensor values
bool force_update_p = false;

// TODO TODO TODO TODO: document
OutputUpdateFunction lcd_fun_override;
byte lcd_override_steps_left = 0;

// TODO TODO TODO TODO TODO: I think I'm using the wrong one :/
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

// needed for strobe effects
uint16_t on_length = 100;
uint16_t off_length = 100;

// Number of NeoPixel LEDS to use
// Used by all patterns (rotary button and IR)
byte np_count = 8;

// Used by all patterns (IR)
bool gamma_correct_p = false;



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

// NOTE: This is AVR specific
uint16_t free_ram() {
    extern uint16_t *__brkval;
    byte last_on_stack;
    // if heap is empty...
    if ((uint16_t)__brkval == 0) {
        // use last-on-stack minus the start of heap
        return ((uint16_t)&last_on_stack) - ((uint16_t)__malloc_heap_start);
    }
    // otherwise, use most recent stack address minus last on heap
    return ((uint16_t)&last_on_stack) - ((uint16_t)__brkval);
}



/* ---------------------------------------------------------
 * HELPER FUNCTIONS                                        */

// NOTE: has side-effects of mutating `control_to_ir` and
// resetting lcd_timeout
bool analog_changed_sufficiently_p(byte previous, byte current,
                                   byte TOLERANCE=ANALOG_DEV_TOLERANCE) {
    if (abs(current - previous) > TOLERANCE) {
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
    if (increment_p) {
        if (current_pattern_fun_index == (NUM_PATTERNS - 1)) {
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
    else if (np_count == 24) {
        np_count = 32;
    }
    else {
        np_count = 8;
        for (byte i = np_count; i < ALL_NP_COUNT; i++) {
            pixels.setPixelColor(i, 0, 0, 0, 0);
        }
        while (!IrReceiver.isIdle()) { }
        pixels.show();
    }
    lcd_timeout = 0;
}

uint16_t kind_of_divide_by(uint16_t thisone, byte divisor) {
    return ((thisone + divisor) >> divisor);
}

void override_LCD_display_with_this(OutputUpdateFunction fun) {
    lcd_fun_override = fun;
    lcd_override_steps_left = LCD_OVERRIDE_LENGTH;
    lcd_timer = 0;
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
    uint16_t freemem = free_ram();
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
    uint16_t freemem = free_ram();
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

// TEMPORARY
void show_on_and_off_length() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("on:  ");
    lcd.print(on_length);
    lcd.setCursor(0, 1);
    lcd.print("off: ");
    lcd.print(off_length);
}



/* ---------------------------------------------------------
 * REMOTE MUTATE FUNCTIONS                                */

// TODO TODO TODO: is there a better way?
// TODO TODO TODO: is it a big deal that I'm not changing
//                 the previous/current values?

// TODO TODO TODO: check all these step sizes

// TODO TODO TODO: explain why the LCD screen overrides are here!
// (because what it shows is specific to the function bound to the
// key, not the actual key that is pressed)

void mutate_brightness_up() {
    brightness = constrain(brightness + 25, 0, 255);
    pixels.setBrightness(brightness);
    override_LCD_display_with_this(show_ir_and_brightness);
}

void mutate_brightness_down() {
    brightness = constrain(brightness - 25, 0, 255);
    pixels.setBrightness(brightness);
    override_LCD_display_with_this(show_ir_and_brightness);
}

void mutate_red_up() {
    byte tmp = current_rgbw[RED_INDEX];
    current_rgbw[RED_INDEX] = constrain(tmp + 10, 0, 255);
    override_LCD_display_with_this(show_rgb_and_gamma);
}

void mutate_red_down() {
    byte tmp = current_rgbw[RED_INDEX];
    current_rgbw[RED_INDEX] = constrain(tmp - 10, 0, 255);
    override_LCD_display_with_this(show_rgb_and_gamma);
}

void mutate_green_up() {
    byte tmp = current_rgbw[GREEN_INDEX];
    current_rgbw[GREEN_INDEX] = constrain(tmp + 10, 0, 255);
    override_LCD_display_with_this(show_rgb_and_gamma);
}

void mutate_green_down() {
    byte tmp = current_rgbw[GREEN_INDEX];
    current_rgbw[GREEN_INDEX] = constrain(tmp - 10, 0, 255);
    override_LCD_display_with_this(show_rgb_and_gamma);
}

void mutate_blue_up() {
    byte tmp = current_rgbw[BLUE_INDEX];
    current_rgbw[BLUE_INDEX] = constrain(tmp + 10, 0, 255);
    override_LCD_display_with_this(show_rgb_and_gamma);
}

void mutate_blue_down() {
    byte tmp = current_rgbw[BLUE_INDEX];
    current_rgbw[BLUE_INDEX] = constrain(tmp - 10, 0, 255);
    override_LCD_display_with_this(show_rgb_and_gamma);
}

void mutate_step_delay_up() {
    step_delay = constrain(step_delay + 5, 0, 255);
    override_LCD_display_with_this(show_step_info);
}

void mutate_step_delay_down() {
    step_delay = constrain(step_delay - 5, 0, 255);
    override_LCD_display_with_this(show_step_info);
}

void mutate_step_delta_up() {
    step_delta = constrain(step_delta + 5, 1, 255);
    override_LCD_display_with_this(show_step_info);
}

void mutate_step_delta_down() {
    step_delta = constrain(step_delta - 5, 1, 255);
    override_LCD_display_with_this(show_step_info);
}

void mutate_on_length_up() {
    on_length = constrain(on_length + kind_of_divide_by(on_length, 2),
                          20, 2000);
    override_LCD_display_with_this(show_on_and_off_length);
}

void mutate_on_length_down() {
    on_length = constrain(on_length - kind_of_divide_by(on_length, 2),
                          20, 2000);
    override_LCD_display_with_this(show_on_and_off_length);
}

void mutate_off_length_up() {
    off_length = constrain(off_length + kind_of_divide_by(off_length, 2),
                           20, 2000);
    override_LCD_display_with_this(show_on_and_off_length);
}

void mutate_off_length_down() {
    off_length = constrain(off_length - kind_of_divide_by(off_length, 2),
                           20, 2000);
    show_on_and_off_length();
    override_LCD_display_with_this(show_on_and_off_length);
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
    if (current_state_CLK != previous_state_CLK  && current_state_CLK == 1) {
        current_state_DT = digitalRead(RE_DT_LAG);
        if (current_state_DT != current_state_CLK) {
            #if DEBUG
            Serial.println(F("supposed to increment"));
            #endif
            update_current_pattern_fun_index(true);
            override_LCD_display_with_this(show_pattern_and_free_mem);
        } else {
            #if DEBUG
            Serial.println(F("supposed to decrement"));
            #endif
            update_current_pattern_fun_index(false);
            override_LCD_display_with_this(show_pattern_and_free_mem);
        }
    }
    previous_state_CLK = current_state_CLK;
    return !pattern_changed_p;
}

// This one stays the same for all patterns
// If the button is pressed, it calls `sw_button_press`, which
// _can_ change based on the pattern / setting
void update_re_button() {
    static uint32_t previous_sw_button_press;
    uint32_t current_millis = millis();
    if (digitalRead(RE_SW_BUTTON)==LOW) {
        if ((current_millis - previous_sw_button_press) > EPSILON) {
            #if DEBUG
            Serial.println(F("button clicked. about to run `sw_button_press`"));
            #endif
            sw_button_press();
        }
        previous_sw_button_press = current_millis;
    }
}

// TODO TODO TODO: document
void update_ir() {
    static uint32_t previous_ir_signal;
    byte command;
    uint32_t current_millis = millis();
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
                    rem_vol_up();
                    break;
                case REM_VOL_DWN:
                    rem_vol_down();
                    break;
                case REM_BACK:
                    update_current_pattern_fun_index(false);
                    override_LCD_display_with_this(show_pattern_and_free_mem);
                    break;
                case REM_FORWARD:
                    update_current_pattern_fun_index(true);
                    override_LCD_display_with_this(show_pattern_and_free_mem);
                    break;
                case REM_FUNC:
                    gamma_correct_p = !gamma_correct_p;
                    override_LCD_display_with_this(show_rgb_and_gamma);
                    break;

                case REM_UP:
                    rem_up();
                    lcd_timer = 0;
                    break;
                case REM_DOWN:
                    rem_down();
                    break;
                case REM_ST:
                    rem_st();
                    break;
                case REM_EQ:
                    rem_eq();
                    break;

                case REM_ZERO:
                    current_pattern_fun_index = 0;
                    pattern_changed_p = true;
                    override_LCD_display_with_this(show_pattern_and_free_mem);
                    break;
                case REM_ONE:
                    current_pattern_fun_index = 1;
                    pattern_changed_p = true;
                    override_LCD_display_with_this(show_pattern_and_free_mem);
                    break;
                case REM_TWO:
                    current_pattern_fun_index = 2;
                    pattern_changed_p = true;
                    override_LCD_display_with_this(show_pattern_and_free_mem);
                    break;
                case REM_THREE:
                    current_pattern_fun_index = 3;
                    pattern_changed_p = true;
                    override_LCD_display_with_this(show_pattern_and_free_mem);
                    break;
                // ACHTUNG: NtS: update
                case REM_FOUR:
                    current_pattern_fun_index = 4;
                    pattern_changed_p = true;
                    override_LCD_display_with_this(show_pattern_and_free_mem);
                    break;
                    // ACHTUNG: NtS: update
                case REM_FIVE:
                    current_pattern_fun_index = 5;
                    pattern_changed_p = true;
                    override_LCD_display_with_this(show_pattern_and_free_mem);
                    break;

                // TODO TODO TODO: temporary
                case REM_SEVEN:
                    override_LCD_display_with_this(show_free_mem_and_timing);
                    break;
                case REM_EIGHT:
                    override_LCD_display_with_this(show_ir_and_brightness);
                    break;
                case REM_NINE:
                    override_LCD_display_with_this(show_step_info);
                    break;

                default:
                    override_LCD_display_with_this(show_home);
            }
        }
        previous_ir_signal = current_millis;
    }
}

    /******* ANALOG *******/

// Used by patterns {0, 1, 2, 4} (NtS)
void update_brightness() {
    static byte previous_brightness;
    byte current_brightness = map(analogRead(THUMB_POT_0_IN), 0, 1023, 255, 1);
    if (analog_changed_sufficiently_p(previous_brightness,
                current_brightness) || force_update_p) {
        previous_brightness = current_brightness;
        brightness = current_brightness;
        pixels.setBrightness(brightness);
        if (!force_update_p)
            override_LCD_display_with_this(show_ir_and_brightness);
    }
}

// Used by patterns {0, 1} (NtS)
void update_step_delay() {
    static byte previous_step_delay;
    byte current_step_delay = map(analogRead(THUMB_POT_1_IN), 0, 1023, 255, 1);
    if (analog_changed_sufficiently_p(previous_step_delay,
                current_step_delay) || force_update_p) {
        previous_step_delay = current_step_delay;
        step_delay = current_step_delay;
        if (!force_update_p)
            override_LCD_display_with_this(show_step_info);
    }
}

// Used by patterns {0, 1} (NtS)
void update_step_delta() {
    // TODO TODO: are these appropriate limits?
    static byte previous_step_delta;
    byte current_step_delta = map(analogRead(THUMB_POT_2_IN), 0, 1023, 100, 1);
    if (analog_changed_sufficiently_p(previous_step_delta,
                current_step_delta) || force_update_p) {
        previous_step_delta = current_step_delta;
        step_delta = current_step_delta;
        if (!force_update_p)
            override_LCD_display_with_this(show_step_info);
    }
}

// Used by patterns {2} (NtS)
void update_red_brightness() {
    static byte previous_red;
    byte current_red = map(analogRead(THUMB_POT_0_IN),
                           0, 1023, max_brightnesses[RED_INDEX], 0);
    if (analog_changed_sufficiently_p(previous_red, current_red) ||
            force_update_p) {
        previous_red = current_red;
        current_rgbw[RED_INDEX] = current_red;
        if (!force_update_p)
            override_LCD_display_with_this(show_rgb_and_gamma);
    }
}

// Used by patterns {2} (NtS)
void update_green_brightness() {
    static byte previous_green;
    byte current_green = map(analogRead(THUMB_POT_1_IN),
                             0, 1023, max_brightnesses[GREEN_INDEX], 0);
    if (analog_changed_sufficiently_p(previous_green, current_green) ||
            force_update_p) {
        previous_green = current_green;
        current_rgbw[GREEN_INDEX] = current_green;
        if (!force_update_p)
            override_LCD_display_with_this(show_rgb_and_gamma);
    }
}

// Used by patterns {2} (NtS)
void update_blue_brightness() {
    static byte previous_blue;
    byte current_blue = map(analogRead(THUMB_POT_2_IN),
                            0, 1023, max_brightnesses[BLUE_INDEX], 0);
    if (analog_changed_sufficiently_p(previous_blue, current_blue) ||
            force_update_p) {
        previous_blue = current_blue;
        current_rgbw[BLUE_INDEX] = current_blue;
        if (!force_update_p)
            override_LCD_display_with_this(show_rgb_and_gamma);
    }
}

// TEMPORARY!!
// TODO TODO TODO TODO: can I use a byte and a multiplier?
void update_on_length() {
    static uint16_t previous_on_length;
    uint16_t current_on_length = map(analogRead(THUMB_POT_1_IN),
                                         0, 1023, 2000, 20);
    if (analog_changed_sufficiently_p(previous_on_length,
                current_on_length, 10) || force_update_p) {
        previous_on_length = current_on_length;
        on_length = current_on_length;
        if (!force_update_p)
            override_LCD_display_with_this(show_on_and_off_length);
    }
}

void update_off_length() {
    static uint16_t previous_off_length;
    uint16_t current_off_length = map(analogRead(THUMB_POT_2_IN),
                                          0, 1023, 2000, 20);
    if (analog_changed_sufficiently_p(previous_off_length,
                current_off_length, 10) || force_update_p) {
        previous_off_length = current_off_length;
        off_length = current_off_length;
        if (!force_update_p)
            override_LCD_display_with_this(show_on_and_off_length);
    }
}




/* ---------------------------------------------------------
 * SHARED FUNCTIONS                                       */

void display_RGBw_colors() {
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

void display_RGBw_zeroes() {
    for (byte i = 0; i < np_count; i++) {
        pixels.setPixelColor(i, 0, 0, 0, 0);
    }
    while (!IrReceiver.isIdle()) { }
    pixels.show();
}

void display_exact_color(byte R, byte G, byte B, byte W) {
    current_rgbw[RED_INDEX]   = R;
    current_rgbw[GREEN_INDEX] = G;
    current_rgbw[BLUE_INDEX]  = B;
    current_rgbw[WHITE_INDEX] = W;
    display_RGBw_colors();
}

void display_dot(byte index, bool mirror_p=true, bool project_p=true) {
    for (byte i = 0; i < np_count; i++) {
        if ((index == i) ||
            (project_p && ((i % 8) == index)) ||
            (mirror_p  && ((7 - i)  == index)) ||
            ((project_p && mirror_p) &&
             ((7 - index) == (i % 8)))) {
            // TODO TODO TODO TODO: seriously need to reduce duplication
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
    }
    while (!IrReceiver.isIdle()) { }
    pixels.show();
}

bool update_all_devices() {
    if (ir_timer > IR_CHECK_EVERY || force_update_p) {
        update_ir();
        ir_timer = 0;
    }
    if (re_timer > RE_CHECK_EVERY || force_update_p) {
        update_rotary_encoder();
        update_re_button();
        re_timer = 0;
    }
    if (pot_timer > POT_CHECK_EVERY || force_update_p) {
        update_thumb_pot_0();
        update_thumb_pot_1();
        update_thumb_pot_2();
        pot_timer = 0;
    }
    if (lcd_timer > LCD_EVERY || force_update_p) {
        if (lcd_timeout >= LCD_TIMEOUT) {
            lcd.noBacklight();
        } else {
            lcd.backlight();
            if (lcd_override_steps_left) {
                lcd_fun_override();
                lcd_override_steps_left--;
            } else {
                update_LCD();
            }
        }
        lcd_timer = 0;
    }
    return !pattern_changed_p;
}



/* ---------------------------------------------------------
 * SHARED FUNCTIONS FOR COLOR MANIPULATION                 */

bool room_to_go_p(bool direction, byte color_index) {
    // UP
    if (direction)
        return (current_rgbw[color_index] <=
                  (max_brightnesses[color_index] - step_delta));
    else
        return (current_rgbw[color_index] >= step_delta);
}

bool shift_color(bool direction, byte color_index, bool reset_timer_p=true) {
    if (step_timer > step_delay) {
        if (room_to_go_p(direction, color_index)) {
            // UP
            if (direction)
                current_rgbw[color_index] = current_rgbw[color_index] +
                                              step_delta;
            else
                current_rgbw[color_index] = current_rgbw[color_index] -
                                              step_delta;
            display_RGBw_colors();
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

// TODO TODO TODO TODO: is this untested??
bool shift_colors(bool direction, byte n_colors, byte* color_indices) {
    bool continue_p = false;
    if (step_timer > step_delay) {
        for (byte i = 0; i < n_colors; i++) {
            byte current_color = color_indices[i];
            if (shift_color(direction, current_color, false))
                continue_p = true;
        }
        step_timer = 0;
        return continue_p;
    }
    return true;
}

// first color goes up, second goes down
bool crossfade_colors(byte color_goes_up, byte color_goes_down) {
    bool more_up_p = room_to_go_p(UP, color_goes_up);
    bool more_down_p = room_to_go_p(DOWN, color_goes_down);
    if (more_up_p || more_down_p) {
        if (step_timer > step_delay) {
            if (more_up_p)
                current_rgbw[color_goes_up] = current_rgbw[color_goes_up] +
                                                step_delta;
            if (more_down_p)
                current_rgbw[color_goes_down] = current_rgbw[color_goes_down] -
                                                  step_delta;
            display_RGBw_colors();

            step_timer = 0;
        }
        return true;
    }
    return false;
}

// TODO TODO TODO TODO: make one function and #define ON/OFF true/false
bool hold_on() {
    if (on_timer > on_length) {
        on_timer = 0;
        return false;
    }
    return true;
}

bool hold_off() {
    if (off_timer > off_length) {
        off_timer = 0;
        return false;
    }
    return true;
}



/* --------------------------------------------------------- */

/* PATTERN 0:
 *
 *   Shifts through all the colors. Brightness, speed, and step size are
 *   parameterized.
 *
 *   Avoids making white
 *
 */

void color_shift_0() {

    #if DEBUG
    Serial.println(F("starting color_shift_0 pattern"));
    #endif

    /* ------- SETUP CODE ------- */
    current_rgbw[RED_INDEX]   = 0; // starts at 011
    current_rgbw[GREEN_INDEX] = 255;
    current_rgbw[BLUE_INDEX]  = 255;

    update_thumb_pot_0  = update_brightness;
    update_thumb_pot_1  = update_step_delay;
    update_thumb_pot_2  = update_step_delta;
    sw_button_press     = update_np_count;
    update_LCD          = show_home;
    rem_vol_up          = mutate_brightness_up;
    rem_vol_down        = mutate_brightness_down;
    rem_up              = mutate_step_delay_up;
    rem_down            = mutate_step_delay_down;
    rem_st              = mutate_step_delta_up;
    rem_eq              = mutate_step_delta_down;

    force_update_p = true;
    update_all_devices();
    force_update_p = false;

    // TODO TODO: why?
    step_timer = 0;

    /* ------- PATTERN LOOP ------- */
    while (!pattern_changed_p) {
        #if PROFILE
        inner_loop_time = 0;
        #endif

        while (update_all_devices() &&
                debug_values() &&
                shift_color(DOWN, GREEN_INDEX)) {}
        while (update_all_devices() &&
                debug_values() &&
                shift_color(UP, RED_INDEX))     {}
        while (update_all_devices() &&
                debug_values() &&
                shift_color(DOWN, BLUE_INDEX))  {}
        while (update_all_devices() &&
                debug_values() &&
                shift_color(UP, GREEN_INDEX))   {}
        while (update_all_devices() &&
                debug_values() &&
                shift_color(DOWN, RED_INDEX))   {}
        while (update_all_devices() &&
                debug_values() &&
                shift_color(UP, BLUE_INDEX))    {}

        #if PROFILE
        current_fun_inner_loop_time = inner_loop_time;
        #endif
    }

    /* ------- TEARDOWN CODE ------- */
    #if DEBUG
    Serial.println(F("ending color_shift_0 pattern"));
    #endif
}



/* --------------------------------------------------------- */

/* PATTERN 1:
 *
 *   Like pattern 0 (`all_color_change_pattern_0`) but maintains
 *   a max of 255 across all channels (collectively) by subtracting
 *   by another channel at the same rate as one channel shifts up
 *
 *   Theoretically, this should keep the brightness somewhat constant.
 *
 *   In reality, it will do no such thing. I know this
 *
 */

void color_shift_constant_255() {

    #if DEBUG
    Serial.println(F("starting color_shift_constant_255 pattern"));
    #endif

    /* ------- SETUP CODE ------- */
    // TODO TODO TODO TODO: this (and others) don't respect max_brightnesses
    current_rgbw[RED_INDEX]   = 255; // starts at 100
    current_rgbw[GREEN_INDEX] = 0;
    current_rgbw[BLUE_INDEX]  = 0;

    update_thumb_pot_0  = update_brightness;
    update_thumb_pot_1  = update_step_delay;
    update_thumb_pot_2  = update_step_delta;
    sw_button_press     = update_np_count;
    update_LCD          = show_home;
    rem_vol_up          = mutate_brightness_up;
    rem_vol_down        = mutate_brightness_down;
    rem_up              = mutate_step_delay_up;
    rem_down            = mutate_step_delay_down;
    rem_st              = mutate_step_delta_up;
    rem_eq              = mutate_step_delta_down;

    force_update_p = true;
    update_all_devices();
    force_update_p = false;

    // TODO TODO: why?
    step_timer = 0;

    /* ------- PATTERN LOOP ------- */
    while (!pattern_changed_p) {
        #if PROFILE
        inner_loop_time = 0;
        #endif

        while (update_all_devices() &&
                debug_values() &&
                crossfade_colors(GREEN_INDEX, RED_INDEX))   {}  // 010
        while (update_all_devices() &&
                debug_values() &&
                crossfade_colors(BLUE_INDEX, GREEN_INDEX))  {}  // 001
        while (update_all_devices() &&
                debug_values() &&
                crossfade_colors(RED_INDEX, BLUE_INDEX))    {}  // 100

        #if PROFILE
        current_fun_inner_loop_time = inner_loop_time;
        #endif
    }

    /* ------- TEARDOWN CODE ------- */
    #if DEBUG
    Serial.println(F("ending color_shift_constant_255 pattern"));
    #endif

}



/* --------------------------------------------------------- */

/* PATTERN 2:
 *
 *   Solid color with option to change the values for each RGB channel
 *
 */

void choose_a_color() {

    #if DEBUG
    Serial.println(F("starting choose_a_color pattern"));
    #endif

    /* ------- SETUP CODE ------- */
    update_thumb_pot_0  = update_red_brightness;
    update_thumb_pot_1  = update_green_brightness;
    update_thumb_pot_2  = update_blue_brightness;
    sw_button_press     = update_np_count;
    update_LCD          = show_rgb_and_gamma;
    rem_vol_up          = mutate_red_up;
    rem_vol_down        = mutate_red_down;
    rem_up              = mutate_green_up;
    rem_down            = mutate_green_down;
    rem_st              = mutate_blue_up;
    rem_eq              = mutate_blue_down;

    force_update_p = true;
    update_all_devices();
    force_update_p = false;
    brightness = 255;
    pixels.setBrightness(brightness);
    display_RGBw_colors();

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
        display_RGBw_colors();

        #if PROFILE
        current_fun_inner_loop_time = inner_loop_time;
        #endif
    }

    /* ------- TEARDOWN CODE ------- */
    #if DEBUG
    Serial.println(F("ending choose_a_color"));
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
    rem_vol_up          = mutate_brightness_up;
    rem_vol_down        = mutate_brightness_down;
    rem_up              = nothing_function;
    rem_down            = nothing_function;
    rem_st              = nothing_function;
    rem_eq              = nothing_function;

    force_update_p = true;
    update_all_devices();
    force_update_p = false;

    current_rgbw[RED_INDEX]   = 0;
    current_rgbw[GREEN_INDEX] = 0;
    current_rgbw[BLUE_INDEX]  = 0;
    current_rgbw[WHITE_INDEX] = 255;

    display_RGBw_colors();

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
        display_RGBw_colors();

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



/* --------------------------------------------------------- */

/* PATTERN 4:
 *
 *  Strobes pink, blue, and purple with control over how
 *  long the lights turn off, and how long they stay on for.
 *
 *    (https://en.wikipedia.org/wiki/Bisexual_lighting)
 *
 */

void bisexual_strobe_pattern() {

    #if DEBUG
    Serial.println(F("starting bisexual strobe pattern"));
    #endif

    /* ------- SETUP CODE ------- */
    update_thumb_pot_0  = update_brightness;
    update_thumb_pot_1  = update_on_length;
    update_thumb_pot_2  = update_off_length;
    sw_button_press     = update_np_count;
    update_LCD          = show_on_and_off_length;
    rem_vol_up          = mutate_brightness_up;
    rem_vol_down        = mutate_brightness_down;
    rem_up              = mutate_on_length_up;
    rem_down            = mutate_on_length_down;
    rem_st              = mutate_off_length_up;
    rem_eq              = mutate_off_length_down;

    /* ------- PATTERN LOOP ------- */
    while (!pattern_changed_p) {
        #if PROFILE
        inner_loop_time = 0;
        #endif

        // TODO TODO TODO TODO NOW: reduce code duplication
        display_exact_color(255, 0, 121, 0);
        on_timer = 0;
        while (update_all_devices() &&
                debug_values() &&
                hold_on())                 {}
        display_RGBw_zeroes();
        off_timer = 0;
        while (update_all_devices() &&
                debug_values() &&
                hold_off())                {}

        display_exact_color(255, 0, 255, 0);
        on_timer = 0;
        while (update_all_devices() &&
                debug_values() &&
                hold_on())                 {}
        display_RGBw_zeroes();
        off_timer = 0;
        while (update_all_devices() &&
                debug_values() &&
                hold_off())                {}

        display_exact_color(0, 0, 255, 0);
        on_timer = 0;
        while (update_all_devices() &&
                debug_values() &&
                hold_on())                 {}
        display_RGBw_zeroes();
        off_timer = 0;
        while (update_all_devices() &&
                debug_values() &&
                hold_off())                {}

        #if PROFILE
        current_fun_inner_loop_time = inner_loop_time;
        #endif
    }

    /* ------- TEARDOWN CODE ------- */
    #if DEBUG
    Serial.println(F("ending bisexual strobe pattern"));
    #endif

}



/* --------------------------------------------------------- */

/* PATTERN 5:
 *
 *  TODO TODO TODO TODO: document
 *
 */

void on_the_bridge() {

    #if DEBUG
    Serial.println(F("starting 'on the bridge' pattern"));
    #endif

    /* ------- SETUP CODE ------- */

    current_rgbw[RED_INDEX]   = 255;
    current_rgbw[GREEN_INDEX] = 82;
    current_rgbw[BLUE_INDEX]  = 9;
    current_rgbw[WHITE_INDEX] = 0;

    update_thumb_pot_0  = update_brightness;
    update_thumb_pot_1  = update_on_length;
    update_thumb_pot_2  = update_off_length;
    sw_button_press     = update_np_count;
    update_LCD          = show_on_and_off_length;
    rem_vol_up          = mutate_brightness_up;
    rem_vol_down        = mutate_brightness_down;
    rem_up              = mutate_on_length_up;
    rem_down            = mutate_on_length_down;
    rem_st              = mutate_off_length_up;
    rem_eq              = mutate_off_length_down;

    /* ------- PATTERN LOOP ------- */
    while (!pattern_changed_p) {
        #if PROFILE
        inner_loop_time = 0;
        #endif

        display_RGBw_zeroes();
        display_dot(3);
        on_timer = 0;
        while (update_all_devices() &&
                debug_values() &&
                hold_on())                 {}
        display_RGBw_zeroes();
        display_dot(2);
        on_timer = 0;
        while (update_all_devices() &&
                debug_values() &&
                hold_on())                 {}
        display_RGBw_zeroes();
        display_dot(1);
        on_timer = 0;
        while (update_all_devices() &&
                debug_values() &&
                hold_on())                 {}
        display_RGBw_zeroes();
        display_dot(0);
        on_timer = 0;
        while (update_all_devices() &&
                debug_values() &&
                hold_on())                 {}
        display_RGBw_zeroes();
        off_timer = 0;
        while (update_all_devices() &&
                debug_values() &&
                hold_off())                {}

        #if PROFILE
        current_fun_inner_loop_time = inner_loop_time;
        #endif
    }

    /* ------- TEARDOWN CODE ------- */
    #if DEBUG
    Serial.println(F("ending 'on the bridge' pattern"));
    #endif

}



/* ---------------------------------------------------------
 * PATTERN FUNCTION ARRAY                                 */

PatternFunction pattern_functions[NUM_PATTERNS] = {
    color_shift_0,
    color_shift_constant_255,
    choose_a_color,
    warm_light_pattern,
    bisexual_strobe_pattern,
    on_the_bridge
};



/* ---------------------------------------------------------
 * SETUP AND MAINLOOP                                     */

void setup() {
    pinMode(RE_CLK,        INPUT);
    pinMode(RE_DT_LAG,     INPUT);
    pinMode(RE_SW_BUTTON,  INPUT_PULLUP);

    pinMode(PHOTORESISTOR, INPUT);

    force_update_p = true;
    update_brightness();
    force_update_p = false;
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
    update_LCD = show_pattern_and_free_mem;
    lcd_timer = 0;
    pattern_functions[current_pattern_fun_index]();

}



/* ---------------------------------------------------------
 * PROGMEM things                                         */

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

