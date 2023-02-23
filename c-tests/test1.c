
#include <stdio.h>
#include <unistd.h>


#define DEBUG 0

#if DEBUG
#define do_something() something()
#else
#define do_something() 1
#endif

/* #define do_something something(); */


int something() {
    printf("doing something\n");
    return 1;
}


int main() {
    while (do_something()) {
        printf("in while loop\n");
        sleep(1);
    }
}
