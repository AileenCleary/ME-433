#include <stdio.h>
#include "pico/stdlib.h"
#include "cam.h"
#include "hardware/pwm.h"
#include "hardware/i2c.h"
#include "font.h"
#include "ssd1306.h"
#include <math.h>
#include <stdlib.h>

#define BASE_SPEED 25
#define MAX_PWM 100

#define BIN1 18 // ENB -> Enable B
#define BIN2 19 // PHB -> Phase B
#define AIN1 20 // ENA -> Enable A
#define AIN2 21 // PHA -> Phase A
#define MODE 13
#define UP 26
#define DOWN 27
#define POWER 28
#define WRAP 6250
#define MIN_SPEED 15
#define FAR_ROW 30  // Look-ahead row


#define I2C_OLED i2c0
#define I2C_OLED_SDA 16 // ***
#define I2C_OLED_SCL 17

#define PWM_FREQ 100 // 100Hz.
#define NUM_ROWS 5

enum Param {P_KP, P_KI, P_KD, P_BASE};
enum Param mode = P_KP;
const float delta = 0.05; // ***
bool state = false;
float base_speed = BASE_SPEED; // Base speed for the motors
volatile bool oled_needs_update = false;
int LeftMotor = BASE_SPEED, RightMotor = BASE_SPEED;
int sign = 1;

float KP = 0.7, KI = 0.0, KD=0.15;
float integral = 0;
float prev_error = 0;

const int rows[NUM_ROWS] = {50, 52, 54, 56, 58};  // much closer to robot
int lost_counter = 0;
bool lost_direction = true;

void OLEDPrint();
void drive_motor(int IN1, int IN2, int speed);
void draw_message(int x, int y, char * m);
void draw_char(int x, int y, unsigned char c);

void drive_motor(int IN1, int IN2, int speed) {
    speed = speed > MAX_PWM ? MAX_PWM : speed;
    speed = speed < -MAX_PWM ? -MAX_PWM : speed;

    if (speed > 0) {
        pwm_set_gpio_level(IN1, (speed * WRAP) / MAX_PWM);
        pwm_set_gpio_level(IN2, 0);
    } else if (speed < 0) {
        pwm_set_gpio_level(IN1, 0);
        pwm_set_gpio_level(IN2, (-speed * WRAP) / MAX_PWM);
    } else {
        pwm_set_gpio_level(IN1, 0);
        pwm_set_gpio_level(IN2, 0);
    }
}

void handle_line_loss() {
    lost_counter++;
    
    if (lost_counter < 3) {
        drive_motor(AIN1, AIN2, 0);
        drive_motor(BIN1, BIN2, 0);
        return;
    }

    int spin_speed = MIN_SPEED + (lost_counter * 2);  // Gradually increase speed
    if (spin_speed > base_speed) spin_speed = base_speed;

    if (lost_direction) {
        drive_motor(AIN1, AIN2, spin_speed);
        drive_motor(BIN1, BIN2, -spin_speed);
    } else {
        drive_motor(AIN1, AIN2, -spin_speed);
        drive_motor(BIN1, BIN2, spin_speed);
    }

    if (lost_counter > 25) {
        lost_counter = 0;
        lost_direction = !lost_direction;
    }
}

int findAverageLine(const int* rows, int count){
    int sum = 0, validCount = 0;
    for (int i = 0; i < count; i++) {
        int x = findLine(rows[i]);
        if (x >= 0) {
            sum += x;
            validCount++;
        }
    }
    return (validCount > 0) ? (sum / validCount) : -1;
}

void gpio_callback(uint gpio, uint32_t events) {
    gpio_callback_cam(gpio, events); // Call the camera GPIO callback
    if (gpio == POWER) {
        state = !state;
        if (state) {
        } else {
            drive_motor(AIN1, AIN2, 0);
            drive_motor(BIN1, BIN2, 0);
        }
    }
    if (gpio == MODE) {
        mode = (enum Param)((mode + 1) % 4);
    }
    if (gpio == DOWN) {
        sign = -1;
    } else if (gpio == UP) {
        sign = 1;
    }
    if (gpio == UP || gpio == DOWN) {
        if (mode == P_KI) {
            KI += delta * sign;
        } else if (mode == P_KD) {
            KD += delta * sign;
        } else if (mode == P_KP) {
            KP += delta * sign;
        } else if (mode == P_BASE) {
            base_speed += sign;
            if (base_speed < 0) {
                base_speed = 0;
            } else if (base_speed > MAX_PWM) {
                base_speed = MAX_PWM;
            }
        }
    }
    //OLEDPrint();
    oled_needs_update = true;  // Mark for update
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

    float divider = 20.0f;
    uint16_t wrap = 6250;

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

void PID_control() {
    
    int total = 0, count = 0;
    for (int i = 0; i < NUM_ROWS; i++) {
        int cx = findLine(rows[i]);
        if (cx >= 0) {
            total += cx;
            count++;
        }
    }

    int near_center = (count > 0) ? (total / count) : -1;
    int far_center = findLine(FAR_ROW);
    int curve_hint = (far_center >= 0 && near_center >= 0) ? (near_center - far_center) : 0; // ***

    if (near_center < 0) {
        handle_line_loss();
        return;
    }
    lost_counter = 0;

    int line_x = near_center; // ***

    int error = (IMAGESIZEX / 2) - line_x;
    if (abs(error) < 5 && count >= NUM_ROWS) {
        LeftMotor += 5;
        RightMotor += 5;
    }

    integral += error;
    integral = fminf(fmaxf(integral, -500), 500);

    float derivative = error - prev_error;
    derivative = fminf(fmaxf(derivative, -50), 50);

    // Optional tuning constants ***
    float boost_KD = 1.5 * KD;
    float curve_threshold = 20;  // You can tune this

    if (abs(curve_hint) > curve_threshold) {
        KD = boost_KD;          // more aggressive turning
        base_speed -= 5;        // slow down slightly for sharp turns
        if (base_speed < MIN_SPEED) base_speed = MIN_SPEED;
    } else {
        KD = 0.15;              // restore default
        base_speed = BASE_SPEED;
    }


    float correction = KP * error + KI * integral + KD * derivative;

    float gain_scale = base_speed / 25.0f;
    if (abs(error) > 30) correction *= gain_scale * 2.0;
    else if (abs(error) > 20) correction *= 1.5;

    prev_error = error;

    int left_speed = base_speed + (int)correction;
    int right_speed = base_speed - (int)correction;

    // Clamp both directions.
    LeftMotor = fminf(fmaxf(left_speed, -MAX_PWM), MAX_PWM);
    RightMotor = fminf(fmaxf(right_speed, -MAX_PWM), MAX_PWM);

    // Avoid deadband for small forward values.
    if (LeftMotor > 0 && LeftMotor < MIN_SPEED) LeftMotor = MIN_SPEED;
    if (RightMotor > 0 && RightMotor < MIN_SPEED) RightMotor = MIN_SPEED;
}

void OLEDPrint() {
    ssd1306_clear();
    char message[50];
    sprintf(message, "KP:%.2f,KI:%.2f,KD:%.2f", KP, KI, KD);
    draw_message(0, 0, message);
    if (mode == P_KI) {
        draw_message(0, 10, "KI");
    } else if (mode == P_KD) {
        draw_message(0, 10, "KD");
    } else if (mode == P_KP) {
        draw_message(0, 10, "KP");
    } else if (mode == P_BASE) {
        char other_msg[20];
        sprintf(other_msg, "Base:%.2f", base_speed);
        draw_message(0, 10, other_msg);
    }
    ssd1306_update();
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

    // while (!stdio_usb_connected()) { // Remove for actual testing.
    //     sleep_ms(100);
    // }
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
    int frame_count = 0;
    while (true) {
        //sleep_ms(1000);

        // char m[10];
        // scanf("%s",m);
        setSaveImage(1);
        while(getSaveImage()==1){}
        convertImage();
        //printImage();
        if (frame_count++ >= 5) {  // adjust as needed
            OLEDPrint();
            frame_count = 0;
        }
        if (oled_needs_update) {
            OLEDPrint();
            oled_needs_update = false;
        }
        
        if (!state) {
            sleep_ms(10);
            drive_motor(AIN1, AIN2, 0);
            drive_motor(BIN1, BIN2, 0);
        } else {
            PID_control();
            drive_motor(AIN1, AIN2, LeftMotor);
            drive_motor(BIN1, BIN2, RightMotor);
        }   
    }
}
