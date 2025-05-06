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
     LOCKED, UNLOCKED, ALARMED, CHANGING, FAILED
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
 
 static int new_combo1[6] = {-1, -1, -1, -1, -1, -1};
 static int new_combo2[6] = {-1, -1, -1, -1, -1, -1};
 static int new_combination[COMBO_LENGTH] = {-1, -1, -1};
 static cowpi_timer_t volatile *timer;

 uint32_t get_microseconds(void) {
    return timer->raw_lower_word;
 }
 
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
                     sprintf(buffer, "alert!");
                     display_string(1, buffer);
                     set_lock_state(ALARMED);
                 } else {
                     sprintf(buffer, "bad try %d", bad_attempts);
                     set_lock_state(FAILED);
                 }
             }
         }
         display_string(1, buffer);
     } else if(get_lock_state() == FAILED){
         for (int i = 0; i < 2; i++) {
             cowpi_illuminate_left_led();
             cowpi_illuminate_right_led();
             uint32_t start = get_microseconds();
             while (get_microseconds() - start < 250000);
                   
             cowpi_deluminate_left_led();
             cowpi_deluminate_right_led();
             start = get_microseconds();
             while (get_microseconds() - start < 250000);
         }
         for (int i = 0; i < COMBO_LENGTH; i++) {
             entered_combination[i] = -1;
         }
         combo_phase = ENTERING_FIRST;
         current_value = 0;
         first_seen_count = 0;
         second_seen_count = 0;
         third_seen_count = 0;
         user_has_interacted = false;
         set_lock_state(LOCKED);
     } else if(get_lock_state() == ALARMED){
         for (int i = 0; i < INT16_MAX; i++) {
             cowpi_illuminate_left_led();
             cowpi_illuminate_right_led();
             uint32_t start = get_microseconds();
             while (get_microseconds() - start < 250000);
                    
             cowpi_deluminate_left_led();
             cowpi_deluminate_right_led();
             start = get_microseconds();
             while (get_microseconds() - start < 250000);
         }
     } else if (get_lock_state() == UNLOCKED) {
         if (cowpi_left_switch_is_in_right_position() && cowpi_right_button_is_pressed()) {
             set_lock_state(CHANGING);
             display_string(1, "enter - - -");
             for (int i = 0; i < 6; i++) {
                new_combo1[i] = -1;
                new_combo2[i] = -1;
             }
         }
         if(cowpi_left_button_is_pressed() && cowpi_right_button_is_pressed()){
             set_lock_state(LOCKED);
         }
     } else if (get_lock_state() == CHANGING) {
        static int input_index = 0;
        static int confirm_phase = 0;


static uint8_t last_key = 0xFF;
char buffer[20];

uint8_t key = cowpi_get_keypress();
if (key != 0xFF && key != last_key && key >= '0' && key <= '9' && input_index < 6) {
    int digit = key - '0';
    if (confirm_phase == 0) {
        new_combo1[input_index] = digit;
    } else {
        new_combo2[input_index] = digit;
    }
    input_index++;
}
last_key = key;

int *combo_ptr = (confirm_phase == 0) ? new_combo1 : new_combo2;
if (input_index == 0) {
    sprintf(buffer, "  -  -  ");
} else if (input_index == 1) {
    sprintf(buffer, "%01d -  -  ", combo_ptr[0]);
} else if (input_index == 2) {
    sprintf(buffer, "%01d%01d-  -  ", combo_ptr[0], combo_ptr[1]);
} else if (input_index == 3) {
    sprintf(buffer, "%01d%01d-%01d -  ", combo_ptr[0], combo_ptr[1], combo_ptr[2]);
} else if (input_index == 4) {
    sprintf(buffer, "%01d%01d-%01d%01d-  ", combo_ptr[0], combo_ptr[1], combo_ptr[2], combo_ptr[3]);
} else if (input_index == 5) {
    sprintf(buffer, "%01d%01d-%01d%01d-%01d ", combo_ptr[0], combo_ptr[1], combo_ptr[2], combo_ptr[3], combo_ptr[4]);
} else if (input_index == 6) {
    sprintf(buffer, "%01d%01d-%01d%01d-%01d%01d", combo_ptr[0], combo_ptr[1], combo_ptr[2], combo_ptr[3], combo_ptr[4], combo_ptr[5]);
}



    if (input_index >= 6 && confirm_phase == 0) {
        confirm_phase = 1;
        input_index = 0;
        display_string(1, "confirm - - -");
    }

        // if(combo_phase == ENTERING_FIRST){
        //     count = 0;
        //     while(count != 2){
        //         if(count == 0){
        //             if(key <= 9){
        //                 new_combo1[0] = key;
        //                 key = 10;
        //                 count++;
        //             }
        //         } else {
        //             if(key <= 9){
        //                 new_combo1[1] = key;
        //                 key = 10;
        //                 count++;
        //                 combo_phase = ENTERING_SECOND;
        //             } 
        //         }
        //     }
        // } else if(combo_phase == ENTERING_SECOND){
        //     count = 0;
        //     while(count != 2){
        //         if(count == 0){
        //             if(key <= 9){
        //                 new_combo1[2] = key;
        //                 key = 10;
        //                 count++;
        //             }
        //         } else {
        //             if(key <= 9){
        //                 new_combo1[3] = key;
        //                 key = 10;
        //                 count++;
        //                 combo_phase = ENTERING_THIRD;
        //             } 
        //         }
        //     }
        // } else if(combo_phase == ENTERING_THIRD){
        //     count = 0;
        //     while(count != 2){
        //         if(count == 0){
        //             if(key <= 9){
        //                 new_combo1[4] = key;
        //                 key = 10;
        //                 count++;
        //             }
        //         } else {
        //             if(key <= 9){
        //                 new_combo1[5] = key;
        //                 key = 10;
        //                 count++;
        //             } 
        //         }
        //     }
        // }
        // if(new_combo1[0] == -1){
        //     sprintf(buffer, "  -  -  ");
        // } else if (combo_phase == ENTERING_FIRST) {
        //     if(count == 1){
        //         sprintf(buffer, "%01d -  -  ", new_combo1[0]);
        //     } else {
        //         sprintf(buffer, "%01d%01d-  -  ", new_combo1[0], new_combo1[1]);
        //     }
        // } else if (combo_phase == ENTERING_SECOND) {
        //     if(count == 1){
        //         sprintf(buffer, "%01d%01d-%01d -  ", new_combo1[0], new_combo1[1], new_combo1[2]);
        //     } else {
        //         sprintf(buffer, "%01d%01d-%01d%01d-  ", new_combo1[0], new_combo1[1], new_combo1[2], new_combo1[3]);
        //     }
        // } else if (combo_phase == ENTERING_THIRD) {
        //     if(count == 1){
        //         sprintf(buffer, "%01d%01d-%01d%01d-%01d ", new_combo1[0], new_combo1[1], new_combo1[2], new_combo1[3], new_combo1[4]);
        //     } else {
        //         sprintf(buffer, "%01d%01d-%01d%01d-%01d%01d", new_combo1[0], new_combo1[1], new_combo1[2], new_combo1[3], new_combo1[4], new_combo1[5]);
        //     }
        // }

        if(cowpi_left_switch_is_in_left_position()){
            if(new_combo1[0] == -1 || new_combo1[1] == -1 || new_combo1[2] == -1 || new_combo1[3] == -1 || new_combo1[4] == -1 || new_combo1[5] == -1 || new_combo2[0] == -1 || new_combo2[1] == -1 || new_combo2[2] == -1 || new_combo2[3] == -1 || new_combo2[4] == -1 || new_combo2[5] == -1){
                sprintf(buffer, "no change");
            } else if(new_combo1[0] != new_combo2[0] || new_combo1[1] != new_combo2[1] || new_combo1[2] != new_combo2[2] || new_combo1[3] != new_combo2[3] || new_combo1[4] != new_combo2[4] || new_combo1[5] != new_combo2[5]){
                sprintf(buffer, "no change");
            } else if(new_combo1[0] == new_combo2[0] && new_combo1[1] == new_combo2[1] && new_combo1[2] == new_combo2[2] && new_combo1[3] == new_combo2[3] && new_combo1[4] == new_combo2[4] && new_combo1[5] == new_combo2[5]){
                new_combination[0] = (new_combo1[0] * 10) + new_combo1[1];
                new_combination[1] = (new_combo1[2] * 10) + new_combo1[3];
                new_combination[2] = (new_combo1[4] * 10) + new_combo1[5];
                if(new_combination[0] > 15 || new_combination[1] > 15 || new_combination[2] > 15){
                    sprintf(buffer, "no change"); 
                } else {
                    combination[0] = new_combination[0];
                    combination[1] = new_combination[1];
                    combination[2] = new_combination[2];
                    sprintf(buffer, "changed"); 
                }
            }
            set_lock_state(UNLOCKED);
        }
        display_string(1, buffer);
     }
 }
 
