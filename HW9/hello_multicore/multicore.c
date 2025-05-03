#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/adc.h"

#define GPIO_LED_PIN 15

char result[64];

void core1_entry() {

    gpio_init(GPIO_LED_PIN);
    gpio_set_dir(GPIO_LED_PIN, GPIO_OUT);
    gpio_put(GPIO_LED_PIN, false);

    adc_init(); 
    adc_gpio_init(26); 
    adc_select_input(0);

    multicore_fifo_push_blocking(1); 

    while (1) {

        uint32_t command = multicore_fifo_pop_blocking();
        
        if (command == 0) {
            uint16_t result_v = adc_read();
            float voltage = 3.3f * ((float)result_v / 4095.0f);
            snprintf(result, sizeof(result), "Voltage: %.2f", voltage);
        }
        else if (command == 1) {
            gpio_put(GPIO_LED_PIN, true);
            snprintf(result, sizeof(result), "LED ON");
        }
        else if (command == 2) {
            gpio_put(GPIO_LED_PIN, false);
            snprintf(result, sizeof(result), "LED OFF");
        }
        else {
            snprintf(result, sizeof(result), "Unknown Command: %c", command);
        }
        multicore_fifo_push_blocking(1);
    }
    
}

int main() {
    stdio_init_all();

    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }

    multicore_launch_core1(core1_entry);

    multicore_fifo_pop_blocking(); 

    int user_input;
    
    while (1) {
        printf("Enter Command: ");
        scanf("%d", &user_input);
        printf("%d\n", user_input);
        
        multicore_fifo_push_blocking((uint32_t)user_input);

        multicore_fifo_pop_blocking();
        printf("%s\n", result);
    }
}
