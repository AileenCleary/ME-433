#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"

#define IN1 14
#define IN2 15
#define PWM_FREQ 100 // 100Hz

int duty_cycle = 0;

void motor_init() {
    gpio_set_function(IN1, GPIO_FUNC_PWM);
    gpio_set_function(IN2, GPIO_FUNC_PWM);

    uint slice1 = pwm_gpio_to_slice_num(IN1);
    uint slice2 = pwm_gpio_to_slice_num(IN2);

    float divider = 100.0f;
    uint16_t wrap = 15000;

    pwm_set_clkdiv(slice1, divider);
    pwm_set_clkdiv(slice2, divider);
    pwm_set_wrap(slice1, wrap);
    pwm_set_wrap(slice2, wrap);
    pwm_set_enabled(slice1, true);
    pwm_set_enabled(slice2, true);
}

void set_motor_speed(int duty) {
    if (duty > 0) {
        pwm_set_gpio_level(IN1, (uint16_t)(duty * 15000) / 100.0f);
        pwm_set_gpio_level(IN2, 0);
    } else if (duty < 0) {
        pwm_set_gpio_level(IN1, 0);
        pwm_set_gpio_level(IN2, (uint16_t)(duty * 15000) / 100.0f);
    } else {
        pwm_set_gpio_level(IN1, 0);
        pwm_set_gpio_level(IN2, 0);
    }
}

int main()
{
    stdio_init_all();

    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }

    motor_init();
    sleep_ms(2000);

    printf("Motor ready. Enter '+' to increase, '-' to decrease duty cycle.\n");

    while (1) {
        char c = getchar();
        if (c == '+') {
            if (duty_cycle < 100) duty_cycle++;
        } else if (c == '-') {
            if (duty_cycle > -100) duty_cycle--;
        } 

        set_motor_speed(duty_cycle);
        printf("Duty cycle: %d%%\n", duty_cycle);
        sleep_ms(100);
    }
}
