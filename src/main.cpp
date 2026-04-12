#include <Arduino.h>
#include <math.h>

// --- Project Structures ---
struct EnergyData {
    float currentAmps;
    float powerWatts;
} sharedData;

// Mutex to prevent Core 0 from reading while Core 1 is writing
SemaphoreHandle_t dataMutex;

// --- Task Handles ---
TaskHandle_t TaskSamplingHandle;
TaskHandle_t TaskOutputHandle;

// --- Constants ---
const int SENSOR_PIN = 34;
const float VOLTAGE_AC = 127.0;
const int SAMPLES = 1000;
const int ADC_OFFSET = 2048;
const float MAX_AMPS = 30.0;

// --- Function Prototypes ---
void TaskSampling(void *pvParameters);
void TaskOutput(void *pvParameters);

// --- Simulation ---
float simCurrentRMS = 15.0; // Simulate 15 Amperes
unsigned long startTime = 0;

void setup() {
    Serial.begin(115200);
    dataMutex = xSemaphoreCreateMutex();

    // Create Task 1: Sampling (Pinned to Core 1)
    xTaskCreatePinnedToCore(
        TaskSampling, "Sampling", 4096, NULL, 1, &TaskSamplingHandle, 1
    );

    // Create Task 2: Output (Pinned to Core 0)
    xTaskCreatePinnedToCore(
        TaskOutput, "Output", 4096, NULL, 1, &TaskOutputHandle, 0
    );
}

void loop() {
    // Empty! FreeRTOS manages everything.
    vTaskDelete(NULL); 
}

// --- Task 1: Electrical Math (High Priority) ---
void TaskSampling(void *pvParameters) {
    startTime = millis();

    for (;;) {
        uint64_t sumSquares = 0;
        for (int i = 0; i < SAMPLES; i++) {
            
            // --- START SIMULATION BLOCK ---
            float t = (micros() / 1000000.0); // Current time in seconds
            
            // Calculate peak amplitude in ADC units for the desired RMS Amps
            // Peak = RMS * sqrt(2). Then scale to ADC range (2048 = 30A)
            float peakADC = (simCurrentRMS * sqrt(2) / MAX_AMPS) * 2048.0;
            
            // Generate the biased sine wave: 2048 + (Peak * sin(2*pi*60*t))
            float virtualSignal = 2048.0 + (peakADC * sin(2.0 * PI * 60.0 * t));
            
            int raw = (int)virtualSignal; 
            // --- END SIMULATION BLOCK ---

            int centered = analogRead(SENSOR_PIN) - ADC_OFFSET;
            sumSquares += (int32_t)centered * centered;
            delayMicroseconds(100); 
        }

        float rmsRaw = sqrt((float)sumSquares / SAMPLES);
        float amps = (rmsRaw / 2048.0) * MAX_AMPS;
        if (amps < 0.10) amps = 0.0;

        // Securely update the shared data
        if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
            sharedData.currentAmps = amps;
            sharedData.powerWatts = amps * VOLTAGE_AC;
            xSemaphoreGive(dataMutex);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10)); // Yield to the OS briefly
    }
}

// --- Task 2: Terminal Output ---
void TaskOutput(void *pvParameters) {
    for (;;) {
        float localAmps, localWatts;

        // Securely read the data
        if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
            localAmps = sharedData.currentAmps;
            localWatts = sharedData.powerWatts;
            xSemaphoreGive(dataMutex);
        }

        Serial.printf(">>> CORE_0 | I: %.3f A | P: %.1f W\n", localAmps, localWatts);
        
        vTaskDelay(pdMS_TO_TICKS(500)); // Output every half second
    }
}