#include <Arduino.h>

// ==========================================
// PIN DEFINITIONS (ESP32-S3)
// ==========================================
// ADC Potentiometers (Simulating Armbands)
#define P1_POT_PIN 12
#define P2_POT_PIN 13

// Physical Buttons (Verified Safe Pins)
#define BTN_START 5
#define BTN_CAL1  6
#define BTN_CAL2  7

// UART Pins for Raspberry Pi / Python Script
#define RXp2 16
#define TXp2 17

// ==========================================
// GAME CONSTANTS & VARIABLES
// ==========================================
const int MAX_SCORE_DIFF = 100;
const int LOOP_INTERVAL = 33; // ~30Hz (1000ms / 30 = 33.3ms)

// Game State Variables
int p1_score = 0;
int p2_score = 0;
int pattern_state = 0; // 0 = Relax, 1 = Flex
int game_state = 0;    // 0=Start, 1=Calib, 2=Playing, 3=GameOver

// Calibration States: 0=Waiting, 1=Calibrating, 2=Ready
int p1_calib_state = 0; 
int p2_calib_state = 0;

// Calibration Thresholds
int p1_min = 4095, p1_max = 0;
int p2_min = 4095, p2_max = 0;
int p1_threshold = 2048; // Will be calculated after calib
int p2_threshold = 2048; 

// Timing Variables
unsigned long last_loop_time = 0;
unsigned long p1_calib_start = 0;
unsigned long p2_calib_start = 0;

// Difficulty & Pattern Variables
unsigned long game_start_time = 0;
unsigned long last_pattern_change = 0;
unsigned long current_pattern_duration = 3000;
int current_level = 0;

// Button Debounce States
bool last_btn_start = true;
bool last_btn_cal1 = true;
bool last_btn_cal2 = true;

// ==========================================
// SETUP
// ==========================================
void setup() {
    Serial.begin(115200);                                 // PC Debugging (Set terminal to 115200!)
    Serial2.begin(115200, SERIAL_8N1, RXp2, TXp2);        // Python/Raspberry Pi UART

    // Set ADC resolution to 12-bit (0-4095)
    analogReadResolution(12);

    // Inputs (Buttons pull to GND, so use PULLUP)
    pinMode(BTN_START, INPUT_PULLUP);
    pinMode(BTN_CAL1, INPUT_PULLUP);
    pinMode(BTN_CAL2, INPUT_PULLUP);

    // Seed randomness using a noisy analog pin
    randomSeed(analogRead(0));

    // Wait a moment for serial to stabilize
    delay(500);
    Serial.println("ESP32 Mainboard Initialized & Ready!");
}

// ==========================================
// HELPER: Generate Pattern Duration
// ==========================================
unsigned long getNextDuration(int level) {
    if (level == 0) return random(2000, 3000);      // Level 0: 2.0s - 3.0s
    if (level == 1) return random(1200, 2000);      // Level 1: 1.2s - 2.0s
    if (level == 2) return random(600, 1200);       // Level 2: 0.6s - 1.2s
    return random(300, 800);                        // Level 3+: 0.3s - 0.8s
}

// ==========================================
// MAIN 30Hz LOOP
// ==========================================
void loop() {
    unsigned long current_time = millis();
     bool p1_is_flexing = false;
    bool p2_is_flexing = false;
    // Run this block exactly at 30Hz
    if (current_time - last_loop_time >= LOOP_INTERVAL) {
        last_loop_time = current_time;

        // 1. Read Inputs
        bool btn_start_pressed = (digitalRead(BTN_START) == LOW && last_btn_start == HIGH);
        bool btn_cal1_pressed  = (digitalRead(BTN_CAL1) == LOW && last_btn_cal1 == HIGH);
        bool btn_cal2_pressed  = (digitalRead(BTN_CAL2) == LOW && last_btn_cal2 == HIGH);
        
        last_btn_start = digitalRead(BTN_START);
        last_btn_cal1  = digitalRead(BTN_CAL1);
        last_btn_cal2  = digitalRead(BTN_CAL2);

        int p1_raw = analogRead(P1_POT_PIN);
        int p2_raw = analogRead(P2_POT_PIN);

        // =======================================================
        // STATE 0: START SCREEN
        // =======================================================
        if (game_state == 0) {
            if (btn_start_pressed) {
                game_state = 1;
                p1_calib_state = 0;
                p2_calib_state = 0;
                Serial.println("Game State -> 1 (Calibration)");
            }
        }
        
        // =======================================================
        // STATE 1: CALIBRATION SCREEN
        // =======================================================
        else if (game_state == 1) {
            
            // Player 1 Trigger
            if (btn_cal1_pressed && p1_calib_state == 0) {
                p1_calib_state = 1; 
                p1_calib_start = current_time;
                p1_min = 4095; p1_max = 0; 
                Serial.println("P1 Calibrating...");
            }
            
            // Player 2 Trigger
            if (btn_cal2_pressed && p2_calib_state == 0) {
                p2_calib_state = 1; 
                p2_calib_start = current_time;
                p2_min = 4095; p2_max = 0; 
                Serial.println("P2 Calibrating...");
            }

            // P1 Calibration Sequence
            if (p1_calib_state == 1) {
                unsigned long elapsed = current_time - p1_calib_start;
                if (elapsed <= 3000) {
                    // PHASE 1: RELAX (Find Minimum)
                    if (p1_raw < p1_min) p1_min = p1_raw;
                } else if (elapsed <= 6000) {
                    // PHASE 2: FLEX (Find Maximum)
                    if (p1_raw > p1_max) p1_max = p1_raw;
                } else {
                    // DONE
                    p1_calib_state = 2; 
                    if (p1_max <= p1_min) p1_max = p1_min + 1; 
                    p1_threshold = p1_min + ((p1_max - p1_min) / 2); 
                    Serial.printf("P1 Ready! Min:%d Max:%d Threshold:%d\n", p1_min, p1_max, p1_threshold);
                }
            }

            // P2 Calibration Sequence
            if (p2_calib_state == 1) {
                unsigned long elapsed = current_time - p2_calib_start;
                if (elapsed <= 3000) {
                    if (p2_raw < p2_min) p2_min = p2_raw;
                } else if (elapsed <= 6000) {
                    if (p2_raw > p2_max) p2_max = p2_raw;
                } else {
                    p2_calib_state = 2; 
                    if (p2_max <= p2_min) p2_max = p2_min + 1; 
                    p2_threshold = p2_min + ((p2_max - p2_min) / 2);
                    Serial.printf("P2 Ready! Min:%d Max:%d Threshold:%d\n", p2_min, p2_max, p2_threshold);
                }
            }

            // Check if both are fully calibrated
            if (p1_calib_state == 2 && p2_calib_state == 2) {
                game_state = 2; 
                game_start_time = current_time;
                last_pattern_change = current_time;
                current_pattern_duration = getNextDuration(0);
                p1_score = 0;
                p2_score = 0;
                Serial.println("Game State -> 2 (Playing!)");
            }
        }

        // =======================================================
        // STATE 2: PLAYING (TUG OF WAR)
        // =======================================================
        else if (game_state == 2) {
            
            // 1. Difficulty Scaling (Level increases every 10,000 ms)
            current_level = (current_time - game_start_time) / 10000;

            // 2. Pattern Timing Logic
            if (current_time - last_pattern_change >= current_pattern_duration) {
                pattern_state = 1 - pattern_state; // Toggle 0 and 1
                last_pattern_change = current_time;
                current_pattern_duration = getNextDuration(current_level);
            }

            // 3. Convert Raw Potentiometer to Boolean "Flexing"
             p1_is_flexing = (p1_raw > p1_threshold);
             p2_is_flexing = (p2_raw > p2_threshold);

            // 4. Scoring Logic
            if (pattern_state == 1) { 
                // FLEX NOW
                if (p1_is_flexing) p1_score++;
                if (p2_is_flexing) p2_score++;
            } else {                  
                // RELAX NOW
                if (!p1_is_flexing) p1_score++;
                if (!p2_is_flexing) p2_score++;
            }

            // 5. Win Condition
            int score_diff = p1_score - p2_score;
            if (abs(score_diff) >= MAX_SCORE_DIFF) {
                game_state = 3; 
                Serial.println("Game State -> 3 (Game Over!)");
            }
        }

        // =======================================================
        // STATE 3: GAME OVER
        // =======================================================
        else if (game_state == 3) {
            if (btn_start_pressed) {
                game_state = 0;
                p1_score = 0;
                p2_score = 0;
                p1_calib_state = 0;
                p2_calib_state = 0;
                Serial.println("Game State -> 0 (Restarted)");
            }
        }

        // =======================================================
        // SERIAL COMMUNICATION TO PYTHON UI
        // =======================================================
        // Format: <P1, P2, PATTERN, STATE, C1, C2, P1_FLEX, P2_FLEX>
        char serialBuffer[64];
        snprintf(serialBuffer, sizeof(serialBuffer), "<%d,%d,%d,%d,%d,%d,%d,%d>\n", 
                 p1_score, 
                 p2_score, 
                 pattern_state, 
                 game_state, 
                 p1_calib_state, 
                 p2_calib_state,
                 p1_is_flexing,   
                 p2_is_flexing);  


        // Send to Pi / Python over USB (Since your PC is plugged into the USB port)
        Serial.print(serialBuffer); 

        // Uncomment this line later when you wire the RX/TX pins directly to the Raspberry Pi!
        // Serial2.print(serialBuffer); 
    }
}