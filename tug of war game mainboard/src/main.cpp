#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// ==========================================
// PIN DEFINITIONS (ESP32-S3)
// ==========================================
#define P1_POT_PIN 12
#define P2_POT_PIN 13

#define BTN_START 5
#define BTN_CAL1  6
#define BTN_CAL2  7

#define RXp2 16
#define TXp2 17

// ==========================================
// FREERTOS VARIABLES
// ==========================================
TaskHandle_t Task_DataReceiver;
TaskHandle_t Task_GameEngine;
SemaphoreHandle_t dataMutex; // Protects shared data between cores

// Shared Data (Core 0 writes, Core 1 reads)
int shared_p1_raw = 0;
int shared_p2_raw = 0;

// ==========================================
// GAME CONSTANTS & VARIABLES
// ==========================================
const int MAX_SCORE_DIFF = 100;

// Game State Variables
int p1_score = 0;
int p2_score = 0;
int pattern_state = 0; 
int game_state = 0;    

int p1_calib_state = 0; 
int p2_calib_state = 0;

int p1_min = 4095, p1_max = 0;
int p2_min = 4095, p2_max = 0;
int p1_threshold = 2048; 
int p2_threshold = 2048; 

unsigned long p1_calib_start = 0;
unsigned long p2_calib_start = 0;

unsigned long game_start_time = 0;
unsigned long last_pattern_change = 0;
unsigned long current_pattern_duration = 3000;
int current_level = 0;

bool last_btn_start = true;
bool last_btn_cal1 = true;
bool last_btn_cal2 = true;

// ==========================================
// HELPER: Generate Pattern Duration
// ==========================================
unsigned long getNextDuration(int level) {
    if (level == 0) return random(2000, 3000);      
    if (level == 1) return random(1200, 2000);      
    if (level == 2) return random(600, 1200);       
    return random(300, 800);                        
}

// ==========================================
// TASK 1: DATA RECEIVER (Pinned to Core 0)
// ==========================================
void DataTask(void *pvParameters) {
    for (;;) {
        // 1. SIMULATE RECEIVING DATA (Read POTs)
        // Later, your ESP-NOW struct data will drop into here!
        int temp_p1 = analogRead(P1_POT_PIN);
        int temp_p2 = analogRead(P2_POT_PIN);

        // 2. LOCK MUTEX AND UPDATE SHARED DATA
        if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
            shared_p1_raw = temp_p1;
            shared_p2_raw = temp_p2;
            xSemaphoreGive(dataMutex); // Unlock
        }

        // Run at ~50Hz (20ms) to simulate the ESP-NOW Armband speed
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// ==========================================
// TASK 2: GAME ENGINE (Pinned to Core 1)
// ==========================================
void GameTask(void *pvParameters) {
    for (;;) {
        unsigned long current_time = millis();
        bool p1_is_flexing = false;
        bool p2_is_flexing = false;

        // 1. Read Physical Buttons
        bool btn_start_pressed = (digitalRead(BTN_START) == LOW && last_btn_start == HIGH);
        bool btn_cal1_pressed  = (digitalRead(BTN_CAL1) == LOW && last_btn_cal1 == HIGH);
        bool btn_cal2_pressed  = (digitalRead(BTN_CAL2) == LOW && last_btn_cal2 == HIGH);
        
        last_btn_start = digitalRead(BTN_START);
        last_btn_cal1  = digitalRead(BTN_CAL1);
        last_btn_cal2  = digitalRead(BTN_CAL2);

        // 2. LOCK MUTEX AND READ SHARED SENSOR DATA
        int p1_raw = 0;
        int p2_raw = 0;
        if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
            p1_raw = shared_p1_raw;
            p2_raw = shared_p2_raw;
            xSemaphoreGive(dataMutex); // Unlock immediately
        }

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
            bool someone_is_calibrating = (p1_calib_state == 1 || p2_calib_state == 1);
    
            if (!someone_is_calibrating) {
                if (btn_cal1_pressed && p1_calib_state == 0) {
                    p1_calib_state = 1; 
                    p1_calib_start = current_time;
                    p1_min = 4095; p1_max = 0; 
                    Serial.println("P1 Calibrating...");
                }
                else if (btn_cal2_pressed && p2_calib_state == 0) {
                    p2_calib_state = 1; 
                    p2_calib_start = current_time;
                    p2_min = 4095; p2_max = 0; 
                    Serial.println("P2 Calibrating...");
                }
            }

            if (p1_calib_state == 1) {
                unsigned long elapsed = current_time - p1_calib_start;
                if (elapsed <= 3000) {
                    if (p1_raw < p1_min) p1_min = p1_raw;
                } else if (elapsed <= 6000) {
                    if (p1_raw > p1_max) p1_max = p1_raw;
                } else {
                    p1_calib_state = 2; 
                    if (p1_max <= p1_min) p1_max = p1_min + 1; 
                    p1_threshold = p1_min + ((p1_max - p1_min) / 2); 
                    Serial.printf("P1 Ready! Min:%d Max:%d Threshold:%d\n", p1_min, p1_max, p1_threshold);
                }
            }

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
            current_level = (current_time - game_start_time) / 20000;

            if (current_time - last_pattern_change >= current_pattern_duration) {
                pattern_state = 1 - pattern_state; 
                last_pattern_change = current_time;
                current_pattern_duration = getNextDuration(current_level);
            }

             p1_is_flexing = (p1_raw > p1_threshold);
             p2_is_flexing = (p2_raw > p2_threshold);

            if (pattern_state == 1) { 
                if (p1_is_flexing) p1_score++;
                if (p2_is_flexing) p2_score++;
            } else {                  
                if (!p1_is_flexing) p1_score++;
                if (!p2_is_flexing) p2_score++;
            }

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
        char serialBuffer[64];
        snprintf(serialBuffer, sizeof(serialBuffer), "<%d,%d,%d,%d,%d,%d,%d,%d>\n", 
                 p1_score, p2_score, pattern_state, game_state, 
                 p1_calib_state, p2_calib_state, p1_is_flexing, p2_is_flexing);  

        Serial.print(serialBuffer); 

        // FreeRTOS Native Delay: Locks this task to exactly ~30Hz (33ms delay)
        // This frees up Core 1 to do background system tasks while waiting.
        vTaskDelay(pdMS_TO_TICKS(33)); 
    }
}

// ==========================================
// SETUP
// ==========================================
void setup() {
    Serial.begin(115200);                                 
    Serial2.begin(115200, SERIAL_8N1, RXp2, TXp2);        

    analogReadResolution(12);

    pinMode(BTN_START, INPUT_PULLUP);
    pinMode(BTN_CAL1, INPUT_PULLUP);
    pinMode(BTN_CAL2, INPUT_PULLUP);

    randomSeed(analogRead(0));

    // Create the Mutex
    dataMutex = xSemaphoreCreateMutex();

    // Start Task 1 (Core 0) - Handles Incoming Data
    xTaskCreatePinnedToCore(
        DataTask,            // Function to implement the task
        "DataTask",          // Name of the task
        4096,                // Stack size in words
        NULL,                // Task input parameter
        1,                   // Priority of the task (1 is standard)
        &Task_DataReceiver,  // Task handle
        0);                  // Core where the task should run (Core 0)

    // Start Task 2 (Core 1) - Handles Game Engine & Serial Out
    xTaskCreatePinnedToCore(
        GameTask, 
        "GameTask", 
        8192,                // Larger stack size because of sprintf & game logic
        NULL, 
        1, 
        &Task_GameEngine, 
        1);                  // Core where the task should run (Core 1)

    Serial.println("ESP32 Dual-Core RTOS Initialized & Ready!");
}

// ==========================================
// STANDARD ARDUINO LOOP (Empty!)
// ==========================================
void loop() {
    // In a FreeRTOS architecture, the standard loop() is left empty.
    // The scheduler handles running our custom tasks forever!
    vTaskDelete(NULL); 
}