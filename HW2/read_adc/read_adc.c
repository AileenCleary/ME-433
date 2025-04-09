#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"

#define GPIO_WATCH_PIN 16
#define GPIO_LED_PIN 14

bool button_state = false;

void pico_led_init(uint gpio, bool state) 
{
    gpio_init(gpio);
    gpio_set_dir(gpio, GPIO_OUT);
    gpio_put(gpio, state);
}

void gpio_callback(uint gpio, uint32_t events)
{
    if (gpio == GPIO_WATCH_PIN) {
        button_state = true;
    }
}

int main()
{
    stdio_init_all();

    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }

    gpio_init(GPIO_WATCH_PIN);
    gpio_pull_up(GPIO_WATCH_PIN);
    pico_led_init(GPIO_LED_PIN, true); 
    gpio_set_irq_enabled_with_callback(GPIO_WATCH_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    while (!button_state) {
        sleep_ms(100);
    }

    gpio_put(GPIO_LED_PIN, false); 

    adc_init(); 
    adc_gpio_init(26); 
    adc_select_input(0);

    button_state = false;
    while (!button_state) {
        int num = 0;
        printf("Enter number of analog samples to read [1-100]: ");
        scanf("%d", &num);
        printf("%d\n", num);

        for (int i = 0; i < num; i++) {
            uint16_t result = adc_read();
            float voltage = 3.3f * ((float)result / 4096.0f);
            printf("ADC Reading %d: %f V\n", i + 1, voltage);
            sleep_ms(1000); 
        }
    }

}
