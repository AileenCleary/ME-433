#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"

#define I2C_PORT i2c0
#define I2C_SDA 16
#define I2C_SCL 17

# define MCP23017_ADDR 0x20 // MCP23017 I2C address
// Fixed Bits : 0b0100 0000 and set AD0-2 to low on board.

# define IODIR 0x00
#define GPIO 0x09
#define OLAT 0x0A

#define LED 25

void mcp_write_reg(uint8_t reg, uint8_t data) {
    uint8_t buf[2];
    buf[0] = reg; 
    buf[1] = data; 

    i2c_write_blocking(I2C_PORT, MCP23017_ADDR, buf, sizeof(buf), false);
}

uint8_t mcp_read_reg(uint8_t reg) {
    uint8_t data;
    i2c_write_blocking(I2C_PORT, MCP23017_ADDR, &reg, 1, true); // Send register address
    i2c_read_blocking(I2C_PORT, MCP23017_ADDR, &data, 1, false); // Read data from register
    return data;
}

void mcp_init() {
    mcp_write_reg(IODIR, 0x7F);
    mcp_write_reg(OLAT, 0x00);
}

bool mcp_read_button() {
    uint8_t data = mcp_read_reg(GPIO);
    return (data & (1 << 0)) == 0; 
}

void mcp_write_led(bool on) {
    uint8_t val = on ? (1 << 7) : 0x00;
    mcp_write_reg(OLAT, val);
}


int main()
{
    stdio_init_all();

    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }

    i2c_init(I2C_PORT, 400*1000);
    
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA); // Maybe change to real life ones..
    gpio_pull_up(I2C_SCL);

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    mcp_init();

    while (true) {
        //toggle heartbeat...

        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        sleep_ms(250);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        sleep_ms(250);

        bool button_pressed = mcp_read_button();
        mcp_write_led(button_pressed);
    }
}
