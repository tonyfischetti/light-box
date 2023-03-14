
#include <stdio.h>
#include <stdint.h>


#define STATION1 8930   // 89.30 MHz
#define STATION2 10350  // 103.5 MHz
#define STATION3 9730   // 97.3 MHz



// radio lib
#define FREQLOW 8700    // I though it was 88 MHz?
#define FREQHIGH 10800   // 108 MHz
#define FREQSTEPS 10     // I thought it was 20

// but _I_ think
/* #define FREQLOW_me 8800    // I though it was 88 MHz? */
#define FREQLOW_me 8750    // I though it was 88 MHz?
#define FREQHIGH_me 10800   // 108 MHz
#define FREQSTEPS_me 20     // I thought it was 20




uint16_t convert_radio(uint16_t astation) {
    uint16_t ret = (astation - FREQLOW) / FREQSTEPS;
    return ret;
}

uint16_t convert_me(uint16_t astation) {
    /* uint16_t ret = (astation - FREQLOW_me) / FREQSTEPS_me; */
    uint16_t ret = (astation - FREQLOW_me) / FREQSTEPS_me;
    return ret;
}

uint16_t convert_spark(uint16_t astation) {
    uint16_t ret = (astation - 8750) / 10; // 9730 -> 98
    return ret;
}

uint16_t convert_right_p(uint16_t astation) {
    uint16_t ret = astation / 200000 - 8750;
    return ret;
}

uint16_t convert_py(uint16_t astation) {
    uint16_t ret = (4 * (astation * 1000000 + 225000) / 32768);
    return ret;
}


// The only (or, rather, first) library I found that seems correct is
// pu2clr/SI470X
///// from there
/*
 * | BAND value     | Description | 
 * | ----------     | ----------- | 
 * |    0           | 00 = 87.5–108 MHz (USA, Europe) (Default) |
 * |    1 (default) | 01 = 76–108 MHz (Japan wide band) | 
 * |    2           | 10 = 76–90 MHz (Japan) | 
 * |    3           | 11 = Reserved |
 * | BAND value     | Description | 
 * | ----------     | ----------- | 
 * |    0           | 00 - 200 kHz (US / Australia, Default) |
 * |    1 (default) | 01 - 100 kHz (Europe / Japan) | 
 * |    2           | 02 - 50 kHz | 
 * |    3           | 03 - Reserved (Do not use)| 
 */

///// from: vmilea/pico_si470x
// FM_BAND_COMMON, /**< 87.5-108 MHz */
// FM_BAND_JAPAN_WIDE, /**< 76-108 MHz */
// FM_BAND_JAPAN, /**< 76-90 MHz */
// FM_CHANNEL_SPACING_200, /**< For Americas, South Korea, Australia. */
// FM_CHANNEL_SPACING_100, /**< For Europe, Japan. */
// FM_CHANNEL_SPACING_50, /**< For Italy. */

// https://github.com/vmilea/pico_si470x/blob/master/fm_si470x/fm_si470x.c#L338


void main() {
    printf("%d	->	%d	->	%d	->	%d	->	%d\n", STATION1,
            convert_radio(STATION1),
            convert_me(STATION1),
            convert_spark(STATION1),
            convert_py(STATION1/100));
    printf("%d	->	%d	->	%d	->	%d	->	%d\n", STATION2,
            convert_radio(STATION2),
            convert_me(STATION2),
            convert_spark(STATION2),
            convert_py(STATION2/100));
    printf("%d	->	%d	->	%d	->	%d	->	%d\n", STATION3,
            convert_radio(STATION3),
            convert_me(STATION3),
            convert_spark(STATION3),
            convert_py(STATION3/100));

    printf("\n%d\n", convert_me(10430));
}


/*
  // Freq(MHz) = 0.200(in USA) * Channel + 87.5MHz
  // 97.3 = 0.2 * Chan + 87.5
  // 9.8 / 0.2 = 49

  int newChannel = channel * 10; //973 * 10 = 9730
  newChannel -= 8750; //9730 - 8750 = 980
  newChannel /= 10; //980 / 10 = 98
*/
