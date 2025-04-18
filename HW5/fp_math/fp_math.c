#include <stdio.h>
#include "pico/stdlib.h"
#include <math.h>

// I AM USING THE PICO 1 WHICH DOES NOT HAVE A FPU, SO IT IS MUCH SLOWER.

int main()
{
    stdio_init_all();

    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }

    volatile float f1, f2;
    printf("Enter two floats to use: ");
    scanf("%f %f", &f1, &f2);
    printf("f1: %f, f2: %f\n", f1, f2);

    volatile float f_add, f_sub, f_mult, f_div;
    absolute_time_t t1, t2;
    uint64_t t;
    double avg_time_ns, clocks;

    t1 = get_absolute_time();
    for (int i = 0; i < 2000; i++) {
        f_add = f1 + f2;
    }
    t2 = get_absolute_time();
    t = to_us_since_boot(t2) - to_us_since_boot(t1);

    avg_time_ns = (t * 1000.0) / 2000.0;
    clocks = avg_time_ns / 7.5188;
    printf("Addition took %.2f clock cycles\n", clocks);
    sleep_ms(200);
    t1 = get_absolute_time();
    for (int i = 0; i < 2000; i++) {
        f_sub = f1 - f2;
    }
    t2 = get_absolute_time();
    t = to_us_since_boot(t2) - to_us_since_boot(t1);

    avg_time_ns = (t * 1000.0) / 2000.0;
    clocks = avg_time_ns / 7.5188;
    printf("Subtraction took %.2f clock cycles\n", clocks);
    sleep_ms(200);
    t1 = get_absolute_time();
    for (int i = 0; i < 2000; i++) {
        f_mult = f1 * f2;
    }
    t2 = get_absolute_time();
    t = to_us_since_boot(t2) - to_us_since_boot(t1);

    avg_time_ns = (t * 1000.0) / 2000.0;
    clocks = avg_time_ns / 7.5188;
    printf("Multiplication took %.2f clock cycles\n", clocks);
    sleep_ms(200);
    if (fabs(f2) < 1e-6) {
        printf("Warning: f2 is very small, may cause divide issues.\n");
    }
    t1 = get_absolute_time();
    for (int i = 0; i < 2000; i++) {
        f_div = f1 / f2;
    }
    t2 = get_absolute_time();
    t = to_us_since_boot(t2) - to_us_since_boot(t1);
    sleep_ms(200);
    avg_time_ns = (t * 1000.0) / 2000.0;
    clocks = avg_time_ns / 7.5188;
    printf("Division took %.2f clock cycles\n", clocks);

}