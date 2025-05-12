#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"

#define SPI_PORT spi0
#define PIN_DI 16
#define PIN_CS 15
#define PIN_SCK 18
#define PIN_DO 19

#define VREF 3.3
#define DAC_RES 1023

#define SAMPLE_RATE 200
#define SINE_FREQ 2
#define TRIANGLE_FREQ 1

int triangle_samples = SAMPLE_RATE / TRIANGLE_FREQ;
int sample_no = 0;

static inline void cs_select(uint cs_pin) {
    asm volatile("nop \n nop \n nop"); // FIXME
    gpio_put(cs_pin, 0);
    asm volatile("nop \n nop \n nop"); // FIXME
}

static inline void cs_deselect(uint cs_pin) {
    asm volatile("nop \n nop \n nop"); // FIXME
    gpio_put(cs_pin, 1);
    asm volatile("nop \n nop \n nop"); // FIXME
}

void write_to_dac(uint16_t voltage, int channel) {

    uint8_t data[2];

    data[0] = 0;
    data[0] |= (channel & 0x01) << 7; // Set channel (0 or 1)
    data[0] |= (0b111 << 4); // Set BUF, GN, and SHDN. Should be A/B 111 0000.
    data[0] |= (voltage >> 6) & 0x0F; // Set voltage value (12 bits) 0000 0011 11.11 1111 // A/B 111 9876

    data[1] = (voltage & 0x3F) << 2; // 5432 10 00

    int len = 2;

    cs_select(PIN_CS);
    spi_write_blocking(SPI_PORT, data, len);
    cs_deselect(PIN_CS);
}

bool create_waves(struct repeating_timer *t) {
    float triangle_phase = (float)(sample_no % triangle_samples) / triangle_samples;
    float triangle_val = (triangle_phase < 0.5f) ? (triangle_phase * 2.0f) : (2.0f * (1 - triangle_phase));
    uint16_t triangle_volt = (uint16_t)(triangle_val * DAC_RES);
    printf("Triangle: %f\n", triangle_val);
    write_to_dac(triangle_volt, 1); // Triangle Wave to B

    float sine_val = sinf(2 * M_PI * SINE_FREQ * (float)sample_no / SAMPLE_RATE);
    sine_val = ((sine_val + 1.0f) / 2.0f);
    printf("Sine: %f\n", sine_val);
    uint16_t sine_volt = (uint16_t)(sine_val * DAC_RES);
    write_to_dac(sine_volt, 0); // Sine Wave to A

    sample_no++;
    return true;
}

int main()
{
    stdio_init_all();

    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }

    spi_init(SPI_PORT, 10000000); // 
    gpio_set_function(PIN_DI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_CS,   GPIO_FUNC_SIO);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_DO, GPIO_FUNC_SPI);
    
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);

    struct repeating_timer timer;
    add_repeating_timer_ms(1000 / SAMPLE_RATE, create_waves, NULL, &timer);

    while (true) {
        tight_loop_contents(); 
    }
}
