/**************************************************************************//**
 *
 * @file lock-controller.c
 *
 * @author (Oliver Triana)
 * @author (Femi Odulate)
 *
 * @brief Code to implement the "combination lock" mode.
 *
 ******************************************************************************/

 #include <CowPi.h>
 #include "display.h"
 #include "lock-controller.h"
 #include "rotary-encoder.h"
 #include "servomotor.h"
 
 #define COMBO_LENGTH 3
 
 typedef enum {
     LOCKED, UNLOCKED, ALARMED, CHANGING
 } lock_state_t;
 
 typedef enum {
     ENTERING_FIRST,
     ENTERING_SECOND,
     ENTERING_THIRD
 } combo_phase_t;
 
 static combo_phase_t combo_phase = ENTERING_FIRST;
 static int entered_combination[COMBO_LENGTH] = {-1, -1, -1};
 static int current_value = 0;
 static int first_seen_count = 0;
 static int second_seen_count = 0;
 static int third_seen_count = 0;
 static int bad_attempts = 0;
 static bool user_has_interacted = false;
 
 static uint8_t combination[3] __attribute__((section (".uninitialized_ram.")));
 static lock_state_t current_state = LOCKED;
 
 // Variables for CHANGING state
 static int new_combo1[COMBO_LENGTH] = {-1, -1, -1};
 static int new_combo2[COMBO_LENGTH] = {-1, -1, -1};
 static int entry_index = 0;
 static bool entering_first = true;
 static cowpi_timer_t volatile *timer;
 
 uint8_t const *get_combination() {
     return combination;
 }
 
 lock_state_t get_lock_state() {
     return current_state;
 }
 
 void set_lock_state(lock_state_t new_state) {
     current_state = new_state;
 }
 
 void force_combination_reset() {
     combination[0] = 5;
     combination[1] = 10;
     combination[2] = 15;
 }
 
 void initialize_lock_controller() {
     set_lock_state(LOCKED);
     combo_phase = ENTERING_FIRST;
     current_value = 0;
     first_seen_count = 0;
     second_seen_count = 0;
     third_seen_count = 0;
     bad_attempts = 0;
     user_has_interacted = false;
     for (int i = 0; i < COMBO_LENGTH; i++) {
         entered_combination[i] = -1;
     }
     display_string(1, "- - -");
     rotate_full_clockwise();
     force_combination_reset();
     timer = (cowpi_timer_t *) (0x40054000);
 }
 
 void initialize_change_mode() {
     entry_index = 0;
     entering_first = true;
     for (int i = 0; i < COMBO_LENGTH; i++) {
         new_combo1[i] = -1;
         new_combo2[i] = -1;
     }
     display_string(1, "enter");
 }
 
 void control_lock() {
     if (get_lock_state() == LOCKED) {
         direction_t dir = get_direction();
 
         if (dir == CLOCKWISE || dir == COUNTERCLOCKWISE) {
             user_has_interacted = true;
         }
 
         if (dir == CLOCKWISE) {
             current_value = (current_value + 1) % 16;
         } else if (dir == COUNTERCLOCKWISE) {
             current_value = (current_value - 1 + 16) % 16;
         }
 
         if (combo_phase == ENTERING_FIRST) {
             if (dir == CLOCKWISE && current_value == combination[0]) {
                 first_seen_count++;
             }
             if (dir == COUNTERCLOCKWISE && entered_combination[0] == -1 && first_seen_count >= 3) {
                 entered_combination[0] = current_value + 1;
                 combo_phase = ENTERING_SECOND;
                 current_value = 0;
             }
         } else if (combo_phase == ENTERING_SECOND) {
             if (dir == COUNTERCLOCKWISE && current_value == combination[1]) {
                 second_seen_count++;
             }
             if (dir == CLOCKWISE && entered_combination[1] == -1 && second_seen_count >= 2) {
                 entered_combination[1] = current_value -1;
                 combo_phase = ENTERING_THIRD;
                 current_value = 0;
             }
         } else if (combo_phase == ENTERING_THIRD) {
              if (dir == CLOCKWISE) {
                  if (current_value == combination[2]) {
                      third_seen_count++;
                  }
              } else if (dir == COUNTERCLOCKWISE) {
                  for (int i = 0; i < COMBO_LENGTH; i++) {
                      entered_combination[i] = -1;
                  }
                  combo_phase = ENTERING_FIRST;
                  current_value = 0;
                  first_seen_count = 0;
                  second_seen_count = 0;
                  third_seen_count = 0;
                  user_has_interacted = false;
              }
          }
 
         char buffer[20];
 
         if (!user_has_interacted) {
             sprintf(buffer, "- - -");
         } else if (combo_phase == ENTERING_FIRST) {
             sprintf(buffer, "%02d-  -  ", current_value);
         } else if (combo_phase == ENTERING_SECOND) {
             sprintf(buffer, "%02d-%02d-  ", entered_combination[0], current_value);
         } else if (combo_phase == ENTERING_THIRD) {
             sprintf(buffer, "%02d-%02d-%02d", entered_combination[0], entered_combination[1], current_value);
         }
 
         if (combo_phase == ENTERING_THIRD && cowpi_left_button_is_pressed()) {
             bool correct = true;
 
             correct = (entered_combination[0] == combination[0] && first_seen_count >= 3)
                    && (entered_combination[1] == combination[1] && second_seen_count == 2)
                    && (current_value == combination[2] && third_seen_count == 1);
 
             if (correct) {
                 set_lock_state(UNLOCKED);
                 sprintf(buffer, "OPEN");
                 rotate_full_counterclockwise();
             } else {
                 bad_attempts++;
                 if (bad_attempts >= 3) {
                     set_lock_state(ALARMED);
                     sprintf(buffer, "alert!");
                 } else {
                     sprintf(buffer, "bad try %d", bad_attempts);
                     for (int i = 0; i < COMBO_LENGTH; i++) {
                         entered_combination[i] = -1;
                     }
                     combo_phase = ENTERING_FIRST;
                     current_value = 0;
                     first_seen_count = 0;
                     second_seen_count = 0;
                     third_seen_count = 0;
                     user_has_interacted = false;
                 }
             }
         }
         display_string(1, buffer);
     } else if (get_lock_state() == UNLOCKED) {
         if (cowpi_left_switch_is_in_right_position() && cowpi_right_button_is_pressed()) {
             set_lock_state(CHANGING);
             initialize_change_mode();
             display_string(1, "CHANGING...");
         } else if(cowpi_left_button_is_pressed() && cowpi_right_button_is_pressed()){
             set_lock_state(LOCKED);
         }
     } else if (get_lock_state() == CHANGING) {
         if (cowpi_get_keypress() != '\0') {
             char key = cowpi_get_keypress();
             int digit = key - '0';
             if (digit < 0 || digit > 15) return;
 
             if (entering_first) {
                 new_combo1[entry_index++] = digit;
                 if (entry_index == COMBO_LENGTH) {
                     display_string(1, "re-enter");
                     entry_index = 0;
                     entering_first = false;
                 }
             } else {
                 new_combo2[entry_index++] = digit;
             }
         }
 
         if (cowpi_left_switch_is_in_left_position()) {
             bool valid = true;
             for (int i = 0; i < COMBO_LENGTH; i++) {
                 if (new_combo1[i] != new_combo2[i] || new_combo1[i] > 15 || new_combo1[i] < 0) {
                     valid = false;
                     break;
                 }
             }
 
             if (entry_index < COMBO_LENGTH || !valid) {
                 display_string(1, "no change");
             } else {
                 for (int i = 0; i < COMBO_LENGTH; i++) {
                     combination[i] = new_combo1[i];
                 }
                 display_string(1, "changed");
             }
             set_lock_state(UNLOCKED);
         }
     }
 }
 