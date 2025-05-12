#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
// #include "hardware/timer.h"

#define SPI_PORT spi0
#define PIN_DI 16
#define DAC_CS 15
#define RAM_CS 14 // ** CHANGE
#define PIN_SCK 18
#define PIN_DO 19

#define VREF 3.3
#define DAC_RES 1023

#define SAMPLE_RATE 200
#define SINE_FREQ 1

volatile bool wave_done = false;

int sample_no = 0;

union FloatInt {
    float f;
    uint32_t i;
};

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

void spi_write_ram(uint16_t addr, float v) {

    uint8_t buf[7];
    buf[0] = 0b00000010; // Write command

    buf[1] = (addr >> 8) & 0xFF; // Address high byte
    buf[2] = addr & 0xFF; // Address low byte

    union FloatInt num;
    num.f = v;

    buf[3] = (num.i >> 24) & 0xFF; // Data high byte
    buf[4] = (num.i >> 16) & 0xFF; // Data mid-high byte
    buf[5] = (num.i >> 8) & 0xFF; // Data mid-low byte
    buf[6] = num.i & 0xFF; // Data low byte

    cs_select(RAM_CS);
    spi_write_blocking(SPI_PORT, buf, sizeof(buf));
    cs_deselect(RAM_CS);
}

float spi_ram_read(uint16_t addr) {

    uint8_t write[7], read[7];
    write[0] = 0b00000011; // Read command

    write[1] = (addr >> 8) & 0xFF; // Address high byte
    write[2] = addr & 0xFF; // Address low byte

    cs_select(RAM_CS);
    spi_write_read_blocking(SPI_PORT, write, read, sizeof(write));
    cs_deselect(RAM_CS);

    union FloatInt num;
    num.i = ((uint32_t)read[3] << 24) | ((uint32_t)read[4] << 16) | ((uint32_t)read[5] << 8) | (uint32_t)read[6];

    return num.f;
}

void spi_ram_init() { 

    uint8_t buf[2];
    buf[0] = 0b1;
    buf[1] = 0b01000000; 

    cs_select(RAM_CS);
    spi_write_blocking(SPI_PORT, buf, 2);
    cs_deselect(RAM_CS);

}

void write_to_dac(uint16_t voltage, int channel) {

    uint8_t data[2];

    data[0] = 0;
    data[0] |= (channel & 0x01) << 7; // Set channel (0 or 1)
    data[0] |= (0b111 << 4); // Set BUF, GN, and SHDN. Should be A/B 111 0000.
    data[0] |= (voltage >> 6) & 0x0F; // Set voltage value (12 bits) 0000 0011 11.11 1111 // A/B 111 9876

    data[1] = (voltage & 0x3F) << 2; // 5432 10 00

    int len = 2;

    cs_select(DAC_CS);
    spi_write_blocking(SPI_PORT, data, len);
    cs_deselect(DAC_CS);
}

bool create_waves(struct repeating_timer *t) {

    if (sample_no >= 1000) {
        wave_done = true;
        return false;
    }

    float sine_val = sinf(2 * M_PI * SINE_FREQ * (float)sample_no / SAMPLE_RATE);
    sine_val = (sine_val + 1.0f) / 2.0f;
    printf("Sine: %f\n", sine_val);

    uint16_t addr = sample_no * 4;
    spi_write_ram(addr, sine_val); // Write to RAM

    sample_no++;
    return true;
}

bool send_waves(struct repeating_timer *t) {

    uint16_t addr = sample_no * 4;
    float val = spi_ram_read(addr); // Read from RAM
    uint16_t sine_volt = (uint16_t)(val * DAC_RES);
    write_to_dac(sine_volt, 0); // Sine Wave to A

    sample_no++;

    if (sample_no >= 1000) {
        sample_no = 0;
    }

    return true;
}

int main()
{
    stdio_init_all();

    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }

    spi_init(SPI_PORT, 20000000); // 
    gpio_set_function(PIN_DI, GPIO_FUNC_SPI);
    gpio_set_function(DAC_CS,   GPIO_FUNC_SIO);
    gpio_set_function(RAM_CS,   GPIO_FUNC_SIO);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_DO, GPIO_FUNC_SPI);
    
    gpio_set_dir(DAC_CS, GPIO_OUT);
    gpio_put(DAC_CS, 1);

    gpio_set_dir(RAM_CS, GPIO_OUT);
    gpio_put(RAM_CS, 1);

    struct repeating_timer timer;
    add_repeating_timer_ms(1000 / SAMPLE_RATE, create_waves, NULL, &timer);

    while (!wave_done) {
        tight_loop_contents(); 
    }

    sample_no = 0;
    struct repeating_timer new_timer;
    add_repeating_timer_ms(1000 / SAMPLE_RATE, send_waves, NULL, &new_timer);

    while (1) {
        tight_loop_contents(); 
    }
}
