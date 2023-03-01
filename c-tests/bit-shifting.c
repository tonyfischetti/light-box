
#include <stdio.h>
#include <stdint.h>


uint32_t b00000001 = 0x01;
uint32_t b00000101 = 0x05;
uint32_t b00011000 = 0x18;


void print_byte(uint8_t byte) {
    printf("(%d)\t0x%02hhx\n", byte, byte);
}

uint32_t kind_of_divide_by_two(uint8_t byte) {
    return ((byte+2) >> 1);
}

uint32_t kind_of_divide_by_four(uint8_t byte) {
    return ((byte+4) >> 2);
}

int main() {

    /* print_byte(b00000001); */
    /*  */
    /* // set bit */
    /* b00000001 |= (1 << 2); */
    /* print_byte(b00000001);  // should be 5 */
    /*  */
    /* // clear bit */
    /* #<{(| print_byte(~(1<<2)); |)}># */
    /* b00000001 &= ~(1 << 2); // should be 1 */
    /* print_byte(b00000001); */
    /*  */
    /* b00011000 &= ~(1 << 4); // should be 8 */
    /* print_byte(b00011000); */
    /*  */
    /* // (setting that back) */
    /* b00011000 |= (1 << 4); */
    /* print_byte(b00011000); */
    /*  */
    /* // get/read bit */
    /* #<{(| print_byte( (b00000001 & (1 << 1)) ); |)}># */
    /* print_byte( ((b00011000 & (1 << 4)) >> 4) ); */
    /*  */
    /* printf("\n"); */
    /*  */
    /* // flip bit */
    /* print_byte( (b00000101 ^ (1 << 2)) ); */
    /* print_byte( (b00000101 ^ (1)) ); */
    /*  */
    /* print_byte( (b00000101 ^ ( 1 << 2) ^ 1) ^ (1 << 4) ^ (1 << 3)); */

    /* print_byte(kind_of_divide_by_two(b00000001)); // should be one */
    /* print_byte(kind_of_divide_by_two(b00000101)); // should be three */
    /* print_byte(kind_of_divide_by_two(b00011000)); // should be 13 */

    for (int i = 0; i < 100; i++) {
        /* printf("%d\t->\t%d\n", i, kind_of_divide_by_two(i)); */
        printf("%d\t->\t%d\n", i, kind_of_divide_by_four(i));
    }

    return 0;
}

