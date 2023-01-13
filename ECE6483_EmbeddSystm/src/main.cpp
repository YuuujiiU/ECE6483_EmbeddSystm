// Project: Just Breathe
// Description: detect stop of breathing for at least 10 seconds using AXDL345
//   accelerometer
//
// Group Members:
// Yujie Yu, Net ID yy3913
// Weijie Li, Net ID wl2369
// Tong Wu, Net ID tw2593
// Junda Ai, Net ID ja4426
//
// Documents
// Dev board dev manual: https://www.st.com/resource/en/user_manual/um1670-discovery-kit-with-stm32f429zi-mcu-stmicroelectronics.pdf
// Accelerometer datasheet: https://www.mouser.com/datasheet/2/389/dm00168691-1798633.pdf
#include "mbed.h"

#include "lcd_display.h"
#include <ADXL345_I2C.h>

// BW_RATE configurations:
// D7 | D6 | D5 | D4               | D3 | D2 | D1 | D0
// ---+----+----+------------------+----+----+----+----
// reserved     | power mode       | data rate
//              | normal operation | 200 Hz (bandwidth 100 Hz)
#define BW_RATE 0x2C
#define BW_RATE_CONFIG 0b000'0'1011

// POWER_CTL configurations:
// D7 | D6  | D5                             | D4         | D3      | D2       | D1 | D0
// ---+-----+--------------------------------+------------+---------+----------+----+----
// reserved | link                           | auto sleep | measure | sleep    | wake up
//          | concurrent inactivity/activity | disabled   | enabled | disabled | 8 Hz of readings in sleep mode
#define POWER_CTL 0x2D
#define POWER_CTL_CONFIG 0b00'0'0'1'0'00

// DATA_FORMAT configurations:
// D7        | D6         | D5             | D4       | D3              | D2              | D1 | D0
// ----------+------------+----------------+----------+-----------------+-----------------+----+----
// self test | SPI        | interrupt mode | reserved | resolution      | justified mode  | g range
// disabled  | 4-wire SPI | active high    |          | full resolution | right-justified | +/-2 g
#define DATA_FORMAT 0x31
#define DATA_FORMAT_CONFIG 0b0'0'0'0'1'0'01

// address of the first output register
#define DATAX0 0x32

// bounds for normal breathing combinated acceleration values
#define BREATHE_UPPER_BOUND 1.025
#define BREATHE_LOWER_BOUND 0.975

// breathe buffer size and threshold
#define BREATHE_BUFFER_SIZE 35
#define BREATHE_THRESHOLD 7

// breathe buffer to take more data points into consideration when determining
// if the user is breathing, reducing false positives
CircularBuffer<bool, BREATHE_BUFFER_SIZE> breathe_buffer;
int breathe_count = 0;

unsigned char LCD_thread_stack[4096];
Thread LCD_thread(osPriorityBelowNormal1, 4096, LCD_thread_stack);

ADXL345 adxl(PB_11, PB_10);

void update_buffer(bool new_data)
{
    // peek the oldest data point in the buffer and push new data (potentially
    // overwriting oldest data point)
    bool peeked_data = false;
    breathe_buffer.peek(peeked_data);
    breathe_buffer.push(new_data);

    if (breathe_buffer.full()) {
        // `breathe_count` change table for when buffer is full
        // peeked_data \ new_data | true | false
        // -----------------------+------+-------
        // true                   | 0    | -1
        // false                  | +1   | 0
        if (peeked_data && !new_data) {
            breathe_count--;
        } else if (!peeked_data && new_data) {
            breathe_count++;
        }
    } else if (new_data) {
        // buffer is not full and new data is true, increment `breathe_count`
        breathe_count++;
    }
}

void read_data_adxllib()
{
    // ADXL345 accelerometer configurations
    adxl.setDataRate(BW_RATE_CONFIG);
    adxl.setPowerControl(POWER_CTL_CONFIG);
    adxl.setDataFormatControl(DATA_FORMAT_CONFIG);
    adxl.setOffset(ADXL345_Z, 0xF3);

    while (1) {
        // sanity check
        // halts program, usually due to loose pin connections
        int devid = adxl.getDevId();
        if (devid != 0xE5) {
            printf("ERROR: DEVID is not 0xE5\n");
            return;
        }

        // i2c read x, y, z axis acceleration data
        int readings[3];
        adxl.getOutput(readings);

        // process output data
        int raw_x = (signed short)(unsigned)readings[0];
        int raw_y = (signed short)(unsigned)readings[1];
        int raw_z = (signed short)(unsigned)readings[2];
        double comb = sqrt(raw_x * raw_x + raw_y * raw_y + raw_z * raw_z) / 256;

        printf("x: %5d\ty: %5d\tz: %5d\t Combined Acc: %.5lf \n", raw_x, raw_y, raw_z, comb);

        // combinated acceleration is out of bounds of normal breathing range
        // write to circular buffer to register one breathe
        if (comb < BREATHE_LOWER_BOUND || comb > BREATHE_UPPER_BOUND) {
            printf("I am BREATHING!!!!\n");
            update_buffer(true);
        } else {
            // combinated acceleration is within normal breathing range
            // write to circular buffer to register one non-breathe
            update_buffer(false);
        }

        // check breathe data points against threshold to reduce false positives
        if (breathe_count > BREATHE_THRESHOLD) {
            printf("---------------- I am BREATHING!!!! +++++++++++++++++++++++++\n");
            breath_detected();
        }

        thread_sleep_for(10);
    }
}

int main()
{
    LCD_thread.start(lcd_run);
    read_data_adxllib();
}
