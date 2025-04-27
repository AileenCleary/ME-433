#include <stdio.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"

#define SERVO_PIN 16 // ***
#define PWM_FREQ 50 // 50Hz
#define MIN_PULSE_US 500 // 0.5ms -> 0 degrees
#define MAX_PULSE_US 2500 // 2.5ms -> 180 degrees

/**
 * NOTE:
 *  Take into consideration if your WS2812 is a RGB or RGBW variant.
 *
 *  If it is RGBW, you need to set IS_RGBW to true and provide 4 bytes per 
 *  pixel (Red, Green, Blue, White) and use urgbw_u32().
 *
 *  If it is RGB, set IS_RGBW to false and provide 3 bytes per pixel (Red,
 *  Green, Blue) and use urgb_u32().
 *
 *  When RGBW is used with urgb_u32(), the White channel will be ignored (off).
 *
 */
#define IS_RGBW false
#define NUM_PIXELS 4
#define WS2812_PIN 15 // ***

// Check the pin is compatible with the platform
#if WS2812_PIN >= NUM_BANK0_GPIOS
#error Attempting to use a pin>=32 on a platform that does not support it
#endif

void pwm_init_servo() {
    gpio_set_function(SERVO_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(SERVO_PIN);

    // PWM frequency is 50Hz. 150MHz / 100 / 30000 = 50Hz
    float divider = 100.0f;
    uint16_t wrap = 30000;

    pwm_set_clkdiv(slice_num, divider); // Set the clock divider to 100
    pwm_set_wrap(slice_num, wrap); // Set the wrap value for 50Hz
    pwm_set_enabled(slice_num, true); // Enable PWM
}

void pwm_set_servo_angle(float angle) {
    // 0.5ms pulse = 2.5% of wrap (30,000) = 750
    // 2.5ms pulse = 12.5% of wrap (30,000) = 3750
    
    if (angle < 0.0f) angle = 0.0f; 
    if (angle > 180.0f) angle = 180.0f; 
    
    uint16_t pulse = 750 + (uint16_t)((angle / 180.0f) * (3750 - 750)); 
    pwm_set_gpio_level(SERVO_PIN, pulse); // Set the pulse width
}

static inline void put_pixel(PIO pio, uint sm, uint32_t pixel_grb) {
    pio_sm_put_blocking(pio, sm, pixel_grb << 8u);
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return
            ((uint32_t) (r) << 16) |
            ((uint32_t) (g) << 8) |
            (uint32_t) (b);
}

typedef struct {
    unsigned char r;
    unsigned char g;
    unsigned char b;
} wsColor; 

// adapted from https://forum.arduino.cc/index.php?topic=8498.0
// hue is a number from 0 to 360 that describes a color on the color wheel
// sat is the saturation level, from 0 to 1, where 1 is full color and 0 is gray
// brightness sets the maximum brightness, from 0 to 1
wsColor HSBtoRGB(float hue, float sat, float brightness) {
    float red = 0.0;
    float green = 0.0;
    float blue = 0.0;

    if (sat == 0.0) {
        red = brightness;
        green = brightness;
        blue = brightness;
    } else {
        if (hue == 360.0) {
            hue = 0;
        }

        int slice = hue / 60.0;
        float hue_frac = (hue / 60.0) - slice;

        float aa = brightness * (1.0 - sat);
        float bb = brightness * (1.0 - sat * hue_frac);
        float cc = brightness * (1.0 - sat * (1.0 - hue_frac));

        switch (slice) {
            case 0:
                red = brightness;
                green = cc;
                blue = aa;
                break;
            case 1:
                red = bb;
                green = brightness;
                blue = aa;
                break;
            case 2:
                red = aa;
                green = brightness;
                blue = cc;
                break;
            case 3:
                red = aa;
                green = bb;
                blue = brightness;
                break;
            case 4:
                red = cc;
                green = aa;
                blue = brightness;
                break;
            case 5:
                red = brightness;
                green = aa;
                blue = bb;
                break;
            default:
                red = 0.0;
                green = 0.0;
                blue = 0.0;
                break;
        }
    }

    unsigned char ired = red * 255.0;
    unsigned char igreen = green * 255.0;
    unsigned char iblue = blue * 255.0;

    wsColor c;
    c.r = ired;
    c.g = igreen;
    c.b = iblue;
    return c;
}

int led_color = 0;
PIO pio_global;
uint sm_global;

bool led_timer_callback(struct repeating_timer *t) {
    float sat = 1.0; // Full saturation
    float brightness = 0.5;

    for (int i = 0; i < NUM_PIXELS; i++) {
        int mod_color = (led_color + (4 * i)) % 360;
        float hue = (float)mod_color;
        wsColor c = HSBtoRGB(hue, sat, brightness);
        uint32_t pixel = urgb_u32(c.r, c.g, c.b);
        put_pixel(pio_global, sm_global, pixel);
    }
    led_color += 1;
    return true;
}

float angle = 0.0f;
float angle_step = 0.1f;
bool up = true;

bool servo_timer_callback(struct repeating_timer *t) {
    pwm_set_servo_angle(angle);
    if (up) {
        angle += angle_step;
        if (angle >= 180.0f) {
            angle = 180.0f; // Limit the angle to 180 degrees{
            up = false; // Change direction
            sleep_ms(100);
        }
    } else {
        angle -= angle_step;
        if (angle <= 0.0f) {
            angle = 0.0f; // Limit the angle to 0 degrees
            up = true; // Change direction
            sleep_ms(100);
        }
    }
    return true;
}


int main() {
    
    stdio_init_all();

    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }

    // pwm_init_servo(); 
    // while (1) {
    //     pwm_set_servo_angle(0.0f); // Set initial angle to 0 degrees
    //     sleep_ms(10000); // Wait for 1 second
    //     pwm_set_servo_angle(90.0f); // Set angle to 90 degrees
    //     sleep_ms(10000); // Wait for 1 second
    //     pwm_set_servo_angle(180.0f);
    //     sleep_ms(10000); // Wait for 1 second
    // }

    PIO pio;
    uint sm;
    uint offset;

    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&ws2812_program, &pio, &sm, &offset, WS2812_PIN, 1, true);
    hard_assert(success);

    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);
    pio_global = pio;
    sm_global = sm;
    pwm_init_servo(); 

    repeating_timer_t led_timer;
    repeating_timer_t servo_timer;

    const float sweep_time = 5.0f;
    const float half_sweep_time = sweep_time / 2.0f;
    const int num_steps = (int)(180.0f / angle_step);
    const int servo_delay_ms = (int)((half_sweep_time * 1000.0f) / num_steps);
    const int led_delay_ms = 14;

    add_repeating_timer_ms(servo_delay_ms, servo_timer_callback, NULL, &servo_timer);
    add_repeating_timer_ms(led_delay_ms, led_timer_callback, NULL, &led_timer);
    
    while (true) {
        tight_loop_contents(); 
    }

    // This will free resources and unload our program
    pio_remove_program_and_unclaim_sm(&ws2812_program, pio, sm, offset);
}
