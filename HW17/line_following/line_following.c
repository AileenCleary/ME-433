#include <stdio.h>
#include "pico/stdlib.h"
#include "cam.h"
#include "hardware/pwm.h"
#include "hardware/i2c.h"
#include "font.h"
#include "ssd1306.h"
#include <math.h>
#include <stdlib.h>

#define LINE_ROW 40
#define BASE_SPEED 80
#define MAX_PWM 100

#define BIN1 18 // BIN1
#define BIN2 19 // BIN2
#define AIN1 20 // AIN1
#define AIN2 21 // AIN2 *** 
#define MODE 13
#define UP 26
#define DOWN 27
#define POWER 28

#define I2C_OLED i2c0
#define I2C_OLED_SDA 16 // ***
#define I2C_OLED_SCL 17

#define PWM_FREQ 100 // 100Hz.
#define NUM_ROWS 5
#define OFFSET 20

enum Param {P_KP, P_KI, P_KD};
enum Param mode = P_KP;
const float delta = 0.05; // ***
bool state = false;

int LINE_THRESHOLD = 100;

int LeftMotor = 0, RightMotor = 0;
int sign = 1;

float KP = 0.5, KI = 0.0, KD=0.2;
float integral = 0;
float prev_error = 0;
bool avg_rows = false;

const int rows[NUM_ROWS] = {28, 30, 32, 34, 36}; // Example rows for line detection

void set_motor_speed(int IN1, int IN2, int speed) {
    pwm_set_gpio_level(IN1, (uint16_t)(speed * 15000) / 100.0f);
    pwm_set_gpio_level(IN2, 0);
}
void gpio_callback(uint gpio, uint32_t events) {
    gpio_callback_cam(gpio, events); // Call the camera GPIO callback
    if (gpio == POWER) {
        state = !state;
        if (state) {
            printf("Power ON\n");
        } else {
            printf("Power OFF\n");
            set_motor_speed(AIN1, AIN2, 0);
            set_motor_speed(BIN1, BIN2, 0);
        }
    }
    if (gpio == MODE) {
        mode = (enum Param)((mode + 1) % 3);
    }
    if (gpio == DOWN) {
        sign = -1;
    } else if (gpio == UP) {
        sign = 1;
    }
    if (gpio == UP || gpio == DOWN) {
        if (mode == P_KP) {
            KP += delta * sign;
        } else if (mode == P_KI) {
            KI += delta * sign;
        } else if (mode == P_KD) {
            KD += delta * sign;
        }
    }
}

void motor_init() {
    gpio_set_function(BIN1, GPIO_FUNC_PWM);
    gpio_set_function(BIN2, GPIO_FUNC_PWM);
    gpio_set_function(AIN1, GPIO_FUNC_PWM);
    gpio_set_function(AIN2, GPIO_FUNC_PWM);

    uint slice1 = pwm_gpio_to_slice_num(BIN1);
    uint slice2 = pwm_gpio_to_slice_num(BIN2);
    uint slice3 = pwm_gpio_to_slice_num(AIN1);
    uint slice4 = pwm_gpio_to_slice_num(AIN2);

    float divider = 100.0f;
    uint16_t wrap = 15000;

    pwm_set_clkdiv(slice1, divider);
    pwm_set_clkdiv(slice2, divider);
    pwm_set_clkdiv(slice3, divider);
    pwm_set_clkdiv(slice4, divider);
    pwm_set_wrap(slice1, wrap);
    pwm_set_wrap(slice2, wrap);
    pwm_set_wrap(slice3, wrap);
    pwm_set_wrap(slice4, wrap);
    pwm_set_enabled(slice1, true);
    pwm_set_enabled(slice2, true);
    pwm_set_enabled(slice3, true);
    pwm_set_enabled(slice4, true);
}


void draw_line(int x) {
    ssd1306_drawPixel(x, LINE_ROW, 1);
    ssd1306_drawPixel(IMAGESIZEX/2, LINE_ROW-10, 1);
    ssd1306_update();
}

void PID_control() {
    int line_x = findLine(IMAGESIZEY / 2);
    
    if (line_x < 0) {
        // No line detected, stop or turn
        printf("No line detected\n");
        set_motor_speed(AIN1, AIN2, 0);
        set_motor_speed(BIN1, BIN2, 0);
        return;
    }

    int error = line_x - (IMAGESIZEX / 2);
    integral += error;
    float derivative = error - prev_error;

    float correction = KP * error + KI * integral + KD * derivative;
    prev_error = error;

    int left_speed = BASE_SPEED + (int)correction;
    int right_speed = BASE_SPEED - (int)correction;

    if (left_speed > MAX_PWM) {
        LeftMotor = MAX_PWM;
    } else if (left_speed < 0) {
        LeftMotor = 0;
    } else {
        LeftMotor = left_speed;
    }
    if (right_speed > MAX_PWM) {
        RightMotor = MAX_PWM;
    } else if (right_speed < 0) {
        RightMotor = 0;
    } else {
        RightMotor = right_speed;
    }
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

int main()
{
    stdio_init_all();

    while (!stdio_usb_connected()) { // Remove for actual testing.
        sleep_ms(100);
    }
    printf("Hello, camera!\n");

    init_camera_pins();
    
    motor_init();
    i2c_init(I2C_OLED, 400*1000);
    gpio_set_function(I2C_OLED_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_OLED_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_OLED_SDA);
    gpio_pull_up(I2C_OLED_SCL);
       
    gpio_init(MODE);
    gpio_pull_up(MODE);
    gpio_init(UP);
    gpio_pull_up(UP);
    gpio_init(DOWN);
    gpio_pull_up(DOWN);
    gpio_init(POWER);
    gpio_pull_up(POWER);
    gpio_set_irq_enabled_with_callback(MODE, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(UP, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(DOWN, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(POWER, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(VS, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(HS, GPIO_IRQ_EDGE_RISE, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(PCLK, GPIO_IRQ_EDGE_RISE, true, &gpio_callback);
    
    ssd1306_setup();
    ssd1306_clear();
    ssd1306_update();
    while (true) {
        //sleep_ms(1000);

        while (!state) {
            sleep_ms(100);
        }
        // char m[10];
        // scanf("%s",m);
        setSaveImage(1);
        while(getSaveImage()==1){}
        convertImage();
        //printImage();
        char message[50];
        sprintf(message, "KP:%.2f,KI:%.2f,KD:%.2f", KP, KI, KD);
        printf("KP: %.2f, KI: %.2f, KD: %.2f\n", KP, KI, KD);
        draw_message(0, 0, message);
        if (mode == P_KP) {
            draw_message(0, 10, "KP");
        } else if (mode == P_KI) {
            draw_message(0, 10, "KI");
        } else if (mode == P_KD) {
            draw_message(0, 10, "KD");
        }
        
        ssd1306_update();
        sleep_ms(100);

        // compute_var_threshold(LINE_ROW); *** TBD
        PID_control();
        set_motor_speed(AIN1, AIN2, LeftMotor);
        set_motor_speed(BIN1, BIN2, RightMotor);
    }
}
