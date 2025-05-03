# Lock Controller Logic – Overview

This file explains the logic used in `lock-controller.c` for our ComboLock project.

## System Flow

### 1. Initialization (`initialize_lock_controller`)
- System starts in the `LOCKED` state.
- Combination is reset to `05-10-15`. (We need to figure out how to get the actual combination, i used this solely for testing)
- Rotary encoder is zeroed (`current_value = 0`).
- Display is cleared with `"- - -"`.
- Servo is rotated fully clockwise to show it's locked.

---

### 2. Combination Entry Phases (`control_lock`)
The combination must be entered in three phases:

#### **Phase 1: ENTERING_FIRST**
- You **rotate clockwise** to scroll numbers.
- If you land on the correct first number **3 times**, then rotate counterclockwise to lock it in.
- If successful: `combo_phase` moves to `ENTERING_SECOND`.

#### **Phase 2: ENTERING_SECOND**
- You **rotate counterclockwise** to scroll numbers.
- If you land on the correct second number **2 times**, then rotate clockwise to lock it in.
- If successful: `combo_phase` moves to `ENTERING_THIRD`.

#### **Phase 3: ENTERING_THIRD**
- You **rotate clockwise** to scroll numbers.
- If you land on the correct third number **once**, it’s saved.
- If you then rotate counterclockwise, the combination resets.

---

### 3. Submitting the Combination
- When the third number is entered, **press the left button** to evaluate.
- It only unlocks if:
  - First number seen ≥ 3 times and is correct
  - Second number seen ≥ 2 times and is correct
  - Third number seen once and is correct

---

## Feedback to User

- If correct: displays `"OPEN"` and unlocks the servo (fully counterclockwise).
- If incorrect:
  - Shows `"bad try 1"` or `"bad try 2"`.
  - After 3 wrong tries: system enters `ALARMED` mode and displays `"alert!"`.

---

## Display Formatting
- Numbers are shown in `XX-XX-XX` format.
- Dashes represent unentered positions.
- Leading zeroes are preserved (e.g., `05` not `5`).

---

## Notes
- Rotary direction determines entry logic.
- No global variables shared between files.
- Display updates once per loop using a shared `buffer` for consistent output.
