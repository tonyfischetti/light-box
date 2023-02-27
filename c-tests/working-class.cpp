
#include <iostream>


#define NUM_PATTERNS 2


using namespace std;



void nothing_function () {
    return;
}

typedef void (*SensorUpdateFunction) ();
SensorUpdateFunction update_thumb_pot_0 = nothing_function;


void update_brightness() {
    cout << "updating brightness!" << endl;
}



class Pattern {
    public:
        string name;
        SensorUpdateFunction update_thumb_pot_0;
        Pattern(string x, SensorUpdateFunction y) {
            name = x;
            update_thumb_pot_0 = y;
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


