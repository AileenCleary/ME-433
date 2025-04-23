#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "font.h"
#include "ssd1306.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"

#define I2C_PORT i2c0
#define I2C_SDA 16
#define I2C_SCL 17



void heartbeat() {
    gpio_put(PICO_DEFAULT_LED_PIN, 1);
    sleep_ms(250);
    gpio_put(PICO_DEFAULT_LED_PIN, 0);
    sleep_ms(250);
}

void draw_char(int x, int y, unsigned char c) {
    int row, col;
    row = c - 0x20;
    col = 0;
    for (col = 0; col < 5; col++) {
        char cur_byte = ASCII[row][col];
        for (int i = 0; i < 8; i++) {
            char pixel = (cur_byte >> i) & 0b1;
            ssd1306_drawPixel(x + col, y + i, pixel);
        }
    }
}

void draw_message(int x, int y, char * m) {
    int i = 0;
    while (m[i] != 0) {
        draw_char(x + i * 5, y, m[i]);
        i++;
    }
}
// time how long 1 frame takes. = seconds/f. fps = inverse..
int main()
{
    stdio_init_all();

    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }

    i2c_init(I2C_PORT, 400*1000);
    
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    adc_init();
    adc_gpio_init(26);
    adc_select_input(0); // ADC0 is GPIO26

    ssd1306_setup();
    ssd1306_clear();
    ssd1306_update();

    while (true) {
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        sleep_ms(250);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        sleep_ms(250);

        char message[50];
        uint16_t adc_value = adc_read();
        float voltage = ((float)adc_value / 4095.0f) * 3.3f; // Convert ADC value to voltage
        sprintf(message, "ADC0: %.2f", voltage);
        // printf("ADC0: %.2f\n", voltage); // test
        unsigned int t1 = to_us_since_boot(get_absolute_time());  
        draw_message(0, 0, message);
        ssd1306_update();
        unsigned int t2 = to_us_since_boot(get_absolute_time());
        unsigned int t = t2 - t1; // time dif in micros.  1 frame per xx ms
        float fps = 1000000.0f / (float)t;

        char fps_message[50];
        sprintf(fps_message, "FPS: %.2f", fps);
        draw_message(0, 24, fps_message);
        ssd1306_update();
        // printf("Frame time: %u us, FPS: %.2f\n", t, fps);
    }
}
