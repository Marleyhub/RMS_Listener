#include <Arduino.h>
#include <math.h>

// --- Project Structures ---
struct EnergyData {
    float currentAmps;
    float powerWatts;
} sharedData;

SemaphoreHandle_t dataMutex;

// --- Task Handles ---
TaskHandle_t TaskSamplingHandle;
TaskHandle_t TaskOutputHandle;

// --- Constants & Hardware Calibration ---
const int SENSOR_PIN = 34;
const float VOLTAGE_AC = 127.0;
const int SAMPLES = 2000;      // Increased for better averaging
const int ADC_OFFSET = 2048;   // Midpoint of 12-bit ADC
volatile int adc_hardware_offset = 2048;

// Calibration breakdown:
const float ADC_VOLTS_PER_STEP = 3.3 / 4096.0;
const float BURDEN_RESISTOR = 22.0;
const float CT_RATIO = 2000.0; // 100A:50mA = 2000:1

// --- Function Prototypes ---
void TaskSampling(void *pvParameters);
void TaskOutput(void *pvParameters);

void setup() {
    Serial.begin(115200);
    
    // --- Auto-Calibration Sequence ---
    Serial.println("[SYSTEM] Calibrating ADC DC-Offset baseline...");
    long running_sum = 0;
    const int calibration_samples = 3000;
    
    for (int i = 0; i < calibration_samples; i++) {
        running_sum += analogRead(SENSOR_PIN);
        delayMicroseconds(100);
    }
    
    // Assign the real physical average to our runtime offset
    adc_hardware_offset = running_sum / calibration_samples;
    Serial.printf("[SYSTEM] Calibration complete. True Hardware Offset: %d\n", adc_hardware_offset);
}

void loop() {
    vTaskDelete(NULL); 
}

// void TaskSampling(void *pvParameters) {
//     for (;;) {
//         // Read the absolute raw value of the pin right now
//         int rawDebug = analogRead(SENSOR_PIN);
        
//         // Calculate voltage seen by the pin
//         float voltageDebug = (rawDebug / 4096.0) * 3.3;

//         Serial.printf("[DEBUG] Raw ADC: %d | Voltage: %.3f V\n", rawDebug, voltageDebug);
        
//         vTaskDelay(pdMS_TO_TICKS(500)); // Sample twice a second
//     }
// }

//--- Task 1: Real-Time Signal Processing (Core 1) ---
void TaskSampling(void *pvParameters) {
    for (;;) {
        uint64_t sumSquares = 0;

        for (int i = 0; i < SAMPLES; i++) {
            // Read actual hardware
            int raw = analogRead(SENSOR_PIN);
            
            // Remove the 1.65V DC Bias
            int centered = raw - ADC_OFFSET;
            
            // Accumulate square of current
            sumSquares += (int32_t)centered * centered;
            
            // 100us = 10kHz sampling frequency
            delayMicroseconds(100); 
        }

        // 1. Calculate statistical RMS from raw units
        float rmsRaw = sqrt((float)sumSquares / SAMPLES);

        // 2. Convert raw units to Volts
        float rmsVoltage = rmsRaw * ADC_VOLTS_PER_STEP;

        // 3. Convert Volts to Primary Amps: (V / R_burden) * CT_Ratio
        float amps = (rmsVoltage / BURDEN_RESISTOR) * CT_RATIO;

        // Digital Noise Gate (filters out ADC jitter at 0A)
        if (amps < 0.15) amps = 0.0;

        // Update shared structure
        if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
            sharedData.currentAmps = amps;
            sharedData.powerWatts = amps * VOLTAGE_AC;
            xSemaphoreGive(dataMutex);
        }
        
        // Small rest to prevent watchdog triggers
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }   
}

// --- Task 2: UI / Terminal Output (Core 0) ---
void TaskOutput(void *pvParameters) {
    for (;;) {
        float localAmps, localWatts;

        if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
            localAmps = sharedData.currentAmps;
            localWatts = sharedData.powerWatts;
            xSemaphoreGive(dataMutex);
        }

        Serial.printf("--- ENERGY MONITOR ---\n");
        Serial.printf("Current: %.3f A\n", localAmps);
        Serial.printf("Power  : %.1f W\n", localWatts);
        Serial.printf("Core   : %d\n", xPortGetCoreID());
        Serial.println("----------------------");
        
        vTaskDelay(pdMS_TO_TICKS(1000)); // Update every second
    }
}