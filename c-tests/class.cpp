
#include <iostream>


#define NUM_PATTERNS 2


using namespace std;



void nothing_function () {
    return;
}

typedef void (*SensorUpdateFunction) ();
SensorUpdateFunction update_thumb_pot_0 = nothing_function;
typedef void (*RemoteIncreaseFunction) (unsigned int*, int, int, int);


void update_brightness() {
    cout << "updating brightness!" << endl;
}

// try to make it a byte
void increase_this_by_this(unsigned int*, int add_this, int lower, int upper) {
    if (add_this < 0 && (-1*add_this) > *var_to_change) {
        *var_to_change = 0;
        // don't worry about opposite case
    }
    else {
        *var_to_change += add_this;
        if (*var_to_change > upper)
            *var_to_change = upper;
        else if (*var_to_change < lower)
            *var_to_change = lower;
    }
}



class Pattern {
    public:
        string name;
        SensorUpdateFunction update_thumb_pot_0;
        RemoteIncreaseFunction vol_up;
        Pattern(string x, SensorUpdateFunction y, RemoteIncreaseFunction z) {
            name = x;
            update_thumb_pot_0 = y;
            vol_up = z;
        }
};

void update_step_delay() {
    cout << "updating step_delay!" << endl;
}


Pattern pattern_0("color shift #0", update_brightness);
Pattern pattern_1("color shift #1", update_step_delay);

Pattern patterns[] = {
    pattern_0,
    pattern_1
};


int main() {

    for (int i = 0; i < NUM_PATTERNS; i++) {
        patterns[i].update_thumb_pot_0();
    }

    return 0;
}


