#include <Arduino.h>
#include <math.h>

struct EnergyData {
    float currentAmps;
    float powerWatts;
} sharedData;

SemaphoreHandle_t dataMutex;
TaskHandle_t TaskSamplingHandle;
TaskHandle_t TaskOutputHandle;

// --- Physical Hardware Map ---
const int SENSOR_PIN = 32;
const float VOLTAGE_AC = 127.0;
const int SAMPLES = 2000;      
const int ADC_OFFSET = 1951; // Confirmed physical midpoint baseline

// --- Precision Calibration Pipeline ---
const float ADC_VOLTS_PER_STEP = 3.3 / 4095.0; 

// Swap your physical 22 ohm resistor out for a 220 or 330 ohm to use these:
const float BURDEN_RESISTOR = 330.0; // Updated to tutorial specification
const float CT_RATIO = 2000.0; 

void TaskSampling(void *pvParameters);
void TaskOutput(void *pvParameters);

void setup() {
    Serial.begin(115200);
    analogReadResolution(12); 
    
    dataMutex = xSemaphoreCreateMutex();

    xTaskCreatePinnedToCore(TaskSampling, "Sampling", 4096, NULL, 1, &TaskSamplingHandle, 1);
    xTaskCreatePinnedToCore(TaskOutput, "Output", 4096, NULL, 1, &TaskOutputHandle, 0);
}

void loop() {
    vTaskDelete(NULL); 
}

void TaskSampling(void *pvParameters) {
    for (;;) {
        uint64_t sumSquares = 0;

        for (int i = 0; i < SAMPLES; i++) {
            int raw = analogRead(SENSOR_PIN);
            int centered = raw - ADC_OFFSET;
            
            sumSquares += (int32_t)centered * centered;
            delayMicroseconds(100); 
        }

        // Statistical RMS Signal Extraction
        float rmsRaw = sqrt((float)sumSquares / SAMPLES);
        float rmsVoltage = rmsRaw * ADC_VOLTS_PER_STEP;
        
        // Final transformation mapping back to primary line amperes
        float amps = (rmsVoltage / BURDEN_RESISTOR) * CT_RATIO;

        // Enhanced Digital Noise Gate
        // Eliminates lingering floor tracking errors when the load is idle
        if (amps < 0.08) {
            amps = 0.0;
        }

        if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
            sharedData.currentAmps = amps;
            sharedData.powerWatts = amps * VOLTAGE_AC;
            xSemaphoreGive(dataMutex);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }   
}

void TaskOutput(void *pvParameters) {
    for (;;) {
        float localAmps, localWatts;

        if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
            localAmps = sharedData.currentAmps;
            localWatts = sharedData.powerWatts;
            xSemaphoreGive(dataMutex);
        }

        Serial.printf("--- ENERGY MONITOR ---\n");
        Serial.printf("Current: %.3f A\n", localAmps - 9.6);
        Serial.printf("Power  : %.1f W\n", localWatts - 1220);
        Serial.printf("Core   : %d\n", xPortGetCoreID());
        Serial.println("----------------------");
        
        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
}