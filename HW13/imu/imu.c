#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "font.h"
#include "ssd1306.h"

// I2C defines
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define I2C_PORT i2c0
#define I2C_SDA 16
#define I2C_SCL 17


// config registers
#define CONFIG 0x1A
#define GYRO_CONFIG 0x1B
#define ACCEL_CONFIG 0x1C
#define PWR_MGMT_1 0x6B
#define PWR_MGMT_2 0x6C
// sensor data registers:
#define ACCEL_XOUT_H 0x3B
#define ACCEL_XOUT_L 0x3C
#define ACCEL_YOUT_H 0x3D
#define ACCEL_YOUT_L 0x3E
#define ACCEL_ZOUT_H 0x3F
#define ACCEL_ZOUT_L 0x40
#define TEMP_OUT_H   0x41
#define TEMP_OUT_L   0x42
#define GYRO_XOUT_H  0x43
#define GYRO_XOUT_L  0x44
#define GYRO_YOUT_H  0x45
#define GYRO_YOUT_L  0x46
#define GYRO_ZOUT_H  0x47
#define GYRO_ZOUT_L  0x48
#define WHO_AM_I     0x75
#define MPU6050_ADDRESS 0x68 // I2C address of the MPU6050

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define CENTER_X (SCREEN_WIDTH / 2)
#define CENTER_Y (SCREEN_HEIGHT / 2)

void mpu_write_reg(uint8_t reg, uint8_t data) {
    uint8_t buf[2];
    buf[0] = reg; 
    buf[1] = data; 
    i2c_write_blocking(I2C_PORT, MPU6050_ADDRESS, buf, sizeof(buf), false);
}

void imu_init()
{
    // Turn the chip on.
    mpu_write_reg(PWR_MGMT_1, 0x00); 

    // Enable accelerometer.
    mpu_write_reg(ACCEL_CONFIG, 0x00); // +/-2g

    // Enable gyroscope.
    mpu_write_reg(GYRO_CONFIG, 0x18); // +/-2000 dps, 0x18 = 0b00011000
}

void heartbeat() 
{
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    while(1) {
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        sleep_ms(250);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        sleep_ms(250); 
    }
}

void i2c_() {
    // Initialize the I2C bus
    i2c_init(I2C_PORT, 400*1000);
    
    // Set the SDA and SCL pins to I2C function
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    
    // Enable pull-up resistors on SDA and SCL lines
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
}

void who_am_i(uint8_t reg)
{
    uint8_t data;
    i2c_write_blocking(I2C_PORT, MPU6050_ADDRESS, &reg, 1, true);
    i2c_read_blocking(I2C_PORT, MPU6050_ADDRESS, &data, 1, false);
    printf("WHO_AM_I: %d\n", data);
    if (data == 0x68) {
        printf("MPU6050 detected\n");
    } else {
        printf("MPU6050 not detected\n");
        heartbeat();
    }
}

void read_accel(int16_t* ax, int16_t* ay) {
    uint8_t reg = ACCEL_XOUT_H;
    uint8_t data[6];
    i2c_write_blocking(I2C_PORT, MPU6050_ADDRESS, &reg, 1, true);
    i2c_read_blocking(I2C_PORT, MPU6050_ADDRESS, data, sizeof(data), false);

    *ax = (data[0] << 8) | data[1];
    *ay = (data[2] << 8) | data[3];
}

void draw_vec(int16_t ax, int16_t ay) {

    float fx = (float)ax / 16384.0;
    float fy = (float)ay / 16384.0;
    int scale = 20;
    int x = (int)(fx * scale);
    int y = (int)(-fy * scale);
    
    ssd1306_drawPixel(CENTER_X, CENTER_Y, 1);
    for (int i = 0; i < abs(x); i++) {
        int shift_x = (x > 0) ? i : -i;
        ssd1306_drawPixel(CENTER_X + shift_x, CENTER_Y, 1);
    }
    for (int i = 0; i < abs(y); i++) {
        int shift_y = (y > 0) ? i : -i;
        ssd1306_drawPixel(CENTER_X, CENTER_Y + shift_y, 1);
    }
    ssd1306_update();
}

int main()
{
    stdio_init_all();

    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }

    i2c_();
    imu_init();

    ssd1306_setup();
    ssd1306_clear();
    ssd1306_update();

    who_am_i(WHO_AM_I);

    int16_t ax, ay;
    while (1) {
        read_accel(&ax, &ay);
        draw_vec(ax, ay);
        sleep_ms(10);
        ssd1306_clear();
        ssd1306_update();
    }
}
