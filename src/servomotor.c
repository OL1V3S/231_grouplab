/**************************************************************************//**
 *
 * @file rotary-encoder.c
 *
 * @author (Oliver Triana)
 * @author (Femi Odulate)
 *
 * @brief Code to control a servomotor.
 *
 ******************************************************************************/

/*
 * ComboLock GroupLab assignment and starter code (c) 2022-24 Christopher A. Bohn
 * ComboLock solution (c) the above-named students
 */

#include <CowPi.h>
#include "servomotor.h"
#include "interrupt_support.h"

#define SERVO_PIN           (22)
#define PULSE_INCREMENT_uS  (500 * 1000)
#define SIGNAL_PERIOD_uS    (20000 * 1000)

static int volatile pulse_width_us;

static void handle_timer_interrupt();

void initialize_servo() {
    cowpi_set_output_pins(1 << SERVO_PIN);
    center_servo();
    register_periodic_timer_ISR(0, PULSE_INCREMENT_uS, handle_timer_interrupt);
}

char *test_servo(char *buffer) {
    if (cowpi_left_button_is_pressed()) {
        center_servo();
        strcpy(buffer, "SERVO: center");
    } else {
        if (cowpi_left_switch_is_in_left_position()) {
            rotate_full_clockwise();
            strcpy(buffer, "SERVO: left");
        } else {
            rotate_full_counterclockwise();
            strcpy(buffer, "SERVO: right");
        }
    }
    return buffer;
}

void center_servo() {
    pulse_width_us = 1500 * 1000;
}

void rotate_full_clockwise() {
    pulse_width_us = 500 * 1000;
}

void rotate_full_counterclockwise() {
    pulse_width_us = 2500 * 1000;
}

static void handle_timer_interrupt() {
    static int rising_edge = 0;
    static int falling_edge = 0;

    if (rising_edge > 0) {
        rising_edge -= PULSE_INCREMENT_uS;
    }
    if (falling_edge > 0) {
        falling_edge -= PULSE_INCREMENT_uS;
    }
    if (rising_edge == 0) {
        cowpi_set_output_pins(1 << 22);
        rising_edge = SIGNAL_PERIOD_uS;
        falling_edge = pulse_width_us;
    }
    if (falling_edge == 0) {
        cowpi_set_output_pins(0);
    }

}

