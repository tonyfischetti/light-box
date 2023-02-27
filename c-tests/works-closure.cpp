
#include <iostream>

using namespace std;



#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))


unsigned int to_mutate_1 = 10;
unsigned int to_mutate_2 = 10;


typedef void (*SensorUpdateFunction) ();

////////// WORKS
auto increase_by_x_closure(unsigned int* var_to_change,
                           int add_this,
                           int lower_bound,
                           int upper_bound) {
    return [=]() {
        // it's unsigned
        if (add_this < 0 && (-1*add_this) > *var_to_change) {
            *var_to_change = 0;
            // don't worry about opposite case
        }
        else {
            *var_to_change += add_this;
            if (*var_to_change > upper_bound)
                *var_to_change = upper_bound;
            else if (*var_to_change < lower_bound)
                *var_to_change = lower_bound;
        }
        return;
    };
}

int main() {

    cout << to_mutate_1 << " " << to_mutate_2 << endl;

    auto increase_by_3 = increase_by_x_closure(&to_mutate_1, 3, 0, 20);
    increase_by_3();
    increase_by_3();

    auto decrease_by_5 = increase_by_x_closure(&to_mutate_2, -5, 0, 15);
    decrease_by_5();

    cout << to_mutate_1 << " " << to_mutate_2 << endl;

    return 0;
}






/*
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
*/
