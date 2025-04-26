/**************************************************************************//**
 *
 * @file rotary-encoder.c
 *
 * @author (Oliver Triana)
 * @author (Femi Odulate)
 *
 * @brief Code to determine the direction that a rotary encoder is turning.
 *
 ******************************************************************************/

/*
 * ComboLock GroupLab assignment and starter code (c) 2022-24 Christopher A. Bohn
 * ComboLock solution (c) the above-named students
 */

#include <CowPi.h>
#include "interrupt_support.h"
#include "rotary-encoder.h"

#define A_WIPER_PIN         (16)
#define B_WIPER_PIN         (A_WIPER_PIN + 1)

#define SIO_GPIO_IN (*(volatile uint32_t *)(0xd0000004))  // RP2040's GPIO input register


typedef enum {
    HIGH_HIGH, HIGH_LOW, LOW_LOW, LOW_HIGH, UNKNOWN
} rotation_state_t;

static rotation_state_t volatile state;
static direction_t volatile direction = STATIONARY;
static int volatile clockwise_count = 0;
static int volatile counterclockwise_count = 0;


static void handle_quadrature_interrupt();

void initialize_rotary_encoder() {

    state = UNKNOWN;
    cowpi_set_pullup_input_pins((1 << A_WIPER_PIN) | (1 << B_WIPER_PIN));
    
    register_pin_ISR((1 << A_WIPER_PIN) | (1 << B_WIPER_PIN), handle_quadrature_interrupt);
}

uint8_t get_quadrature() {
    
    uint32_t gpio_state = SIO_GPIO_IN;

    uint8_t a = (gpio_state >> A_WIPER_PIN) & 0x01;
    uint8_t b = (gpio_state >> B_WIPER_PIN) & 0x01;

    return (b << 1) | a;
}

char *count_rotations(char *buffer) {
    
    sprintf(buffer, "CW:%d CCW:%d", clockwise_count, counterclockwise_count);

    return buffer;
}

direction_t get_direction() {
    direction_t current_direction = direction;
    direction = STATIONARY;
    return current_direction;
}

static void handle_quadrature_interrupt() {
    static rotation_state_t last_state = HIGH_HIGH;
    static rotation_state_t state_before_last = HIGH_HIGH;

    uint8_t quadrature = get_quadrature();

    rotation_state_t current_state;
    switch (quadrature) {
        case 0b00: current_state = LOW_LOW; break;
        case 0b01: current_state = LOW_HIGH; break;
        case 0b10: current_state = HIGH_LOW; break;
        case 0b11: current_state = HIGH_HIGH; break;
        default:   current_state = UNKNOWN; break;
    }

    if (current_state == LOW_LOW) {
        if (last_state == HIGH_LOW && state_before_last == HIGH_HIGH) {
            clockwise_count++;
            direction = CLOCKWISE;
        } else if (last_state == LOW_HIGH && state_before_last == HIGH_HIGH) {
            counterclockwise_count++;
            direction = COUNTERCLOCKWISE;
        }
    }

    state_before_last = last_state;
    last_state = current_state;
}


