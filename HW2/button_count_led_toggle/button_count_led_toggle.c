#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#define GPIO_WATCH_PIN 16
#define GPIO_LED_PIN 14

int counter = 0;
bool led_state = false;

void pico_led_init(uint gpio) 
{
    gpio_init(gpio);
    gpio_set_dir(gpio, GPIO_OUT);
    gpio_put(gpio, false);
}

void gpio_callback(uint gpio, uint32_t events)
{
    if (gpio == GPIO_WATCH_PIN) {
        counter++;
        led_state = !led_state;
        gpio_put(GPIO_LED_PIN, led_state);
        printf("Button pressed %d times\n", counter);
    }
}

int main()
{
    stdio_init_all();
    sleep_ms(2000);
    printf("Counter started\n");

    pico_led_init(GPIO_LED_PIN);
    gpio_init(GPIO_WATCH_PIN);
    gpio_pull_up(GPIO_WATCH_PIN);
    gpio_set_irq_enabled_with_callback(GPIO_WATCH_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    
    while (1);
}
