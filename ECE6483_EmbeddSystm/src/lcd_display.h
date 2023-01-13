#ifndef _LCD_DISPLAY
#define _LCD_DISPLAY

#include <mbed.h>
// this file has all the functions for interacting
// with the screen
#include "drivers/LCD_DISCO_F429ZI.h"
#define BACKGROUND 1
#define FOREGROUND 0
#define GRAPH_PADDING 15

#define BUFFER_SIZE 230
#define ONE_BLINK 1UL << 0
#define FULL 1UL << 1

#define WARNING_TRIGGER 1UL << 2
#define WARNING 1UL << 3
#define NORMAL_TRIGGER 1UL << 4

LCD_DISCO_F429ZI lcd;
// buffer for holding displayed text strings
char display_buf[3][60];
uint32_t graph_width = lcd.GetXSize() - 2 * GRAPH_PADDING;
uint32_t graph_height = graph_width;

// interrupt setup for debugging using user btn
InterruptIn intB(PA_0, PullDown);

Timeout timeout;

// stack used by the threads
unsigned char blink_thread_stack[512];
unsigned char full_thread_stack[512];

// threads for blinking screen and normal screen
Thread blink_thread(osPriorityBelowNormal1, 512, blink_thread_stack);
Thread full_thread(osPriorityBelowNormal1, 512, full_thread_stack);

// semaphore used to protect the new_values buffer
Semaphore blink_semaphore(0, BUFFER_SIZE);
Semaphore full_semaphore(0, BUFFER_SIZE);

EventFlags status_flags;

bool _cnt = true;

//--------------- LCD +++++++++++++++
// sets the background layer to
// white and draw the outer ring
// on the screen
void setup_background_layer()
{
    lcd.SelectLayer(BACKGROUND);
    lcd.Clear(LCD_COLOR_WHITE);
    lcd.SetBackColor(LCD_COLOR_WHITE);
    lcd.SetTextColor(LCD_COLOR_DARKGRAY);
    lcd.SetLayerVisible(BACKGROUND, ENABLE);
    lcd.SetTransparency(BACKGROUND, 0x9Fu);
    for (uint16_t i = lcd.GetXSize() / 2 - GRAPH_PADDING + 1; i < lcd.GetXSize() / 2; i++) {
        lcd.DrawCircle(lcd.GetXSize() / 2, lcd.GetYSize() / 2, i);
    }
}

// sets the foreground layer with @param color
// to draw inner circle
void setup_foreground_layer(uint32_t color)
{
    lcd.SelectLayer(FOREGROUND);
    lcd.Clear(LCD_COLOR_BLACK);
    lcd.SetBackColor(LCD_COLOR_BLACK);
    lcd.SetTextColor(color);
}

// Helper function to draw circle
void draw_circle()
{
    lcd.SelectLayer(FOREGROUND);
    lcd.FillCircle(lcd.GetXSize() / 2, lcd.GetYSize() / 2, lcd.GetXSize() / 2 - GRAPH_PADDING);
}

//--------------- callback & ISR +++++++++++++++
// ISR for button interrupt
void i2c_cb_rise()
{
    _cnt = !_cnt;
    if (_cnt) {
        status_flags.set(WARNING_TRIGGER);
    } else {
        status_flags.clear(WARNING_TRIGGER);
    }
}

// 10s timeout expired callback
// set the warning flag to flash the red light on the LCD screen
void timeout_expired_cb()
{
    status_flags.set(WARNING_TRIGGER);
}

// clear flags and reset timeout when a breathe is registered
void breath_detected()
{
    // clear the warning flag
    if (status_flags.get() && WARNING_TRIGGER)
        status_flags.clear(WARNING_TRIGGER);

    // reset the timeout
    timeout.detach();
    timeout.attach(timeout_expired_cb, 12s);
}

//--------------- thread +++++++++++++++
// Blink warning thread
// blocked when not notified by main thread
// blink red circle when wake up
void blink_thread_proc()
{
    while (1) {
        // wait for main thread to release a semaphore,
        // to indicate a new sample is ready to be graphed
        blink_semaphore.acquire();
        setup_foreground_layer(LCD_COLOR_RED);
        printf("Warning!\n");
        lcd.SelectLayer(FOREGROUND);
        draw_circle();
        thread_sleep_for(500);
        lcd.SelectLayer(FOREGROUND);
        lcd.Clear(LCD_COLOR_BLACK);
        thread_sleep_for(200);
        status_flags.set(ONE_BLINK);
    }
}

// Full normal thread
// blocked when not notified by main thread
// stay blue when wake up
void full_thread_proc()
{
    while (1) {
        // wait for main thread to release a semaphore to
        // indicate normal state.
        full_semaphore.acquire();
        setup_foreground_layer(LCD_COLOR_DARKBLUE);
        printf("Normal\n");
        lcd.SelectLayer(FOREGROUND);
        draw_circle();
    }
}

// LCD center logic control thread
void lcd_run()
{
    // setup threads and background
    setup_background_layer();
    intB.rise(&i2c_cb_rise);
    blink_thread.start(blink_thread_proc);
    full_thread.start(full_thread_proc);
    while (1) {
        if (status_flags.get() && WARNING_TRIGGER) {
            // warning flag detected, wake up
            // blink thread and keeps detecting warning
            // flag.
            if (blink_semaphore.release() != osOK) {
                MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, MBED_ERROR_CODE_OUT_OF_MEMORY), "semaphore overflow\r\n");
            }
            status_flags.wait_all(ONE_BLINK);
        } else {
            // warning flag not detected, keeps in
            // normal thread and register breath until
            // warning flag established
            breath_detected();
            if (full_semaphore.release() != osOK) {
                MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, MBED_ERROR_CODE_OUT_OF_MEMORY), "semaphore overflow\r\n");
            }
            status_flags.wait_all(WARNING_TRIGGER, osWaitForever, false);
        }
    }
}

#endif
