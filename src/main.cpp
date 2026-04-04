#include <Arduino.h>
#include <math.h>

// --- HARDWARE CONFIGURATION ---
const int SENSOR_PIN = 34;      // ESP32 ADC1_CH6
const float ADC_RES = 4095.0;   // 12-bit Resolution
const int ADC_OFFSET = 2048;    // Center bias (1.65V)

// --- SENSOR & GRID CONFIG ---
const float MAX_AMPS = 30.0;    // Rating of your SCT-013-030
const float VOLTAGE_AC = 127.0; // Local Grid Voltage
const float NO_LOAD_CUTOFF = 0.08; // Ignore anything below 80mA (noise)

// --- SAMPLING CONFIG ---
const int SAMPLES = 1000;       // Total samples per window

void setup() {
    Serial.begin(115200);
    pinMode(SENSOR_PIN, INPUT);
    delay(1000);
    Serial.println("STATUS: ESP32_RMS_LISTENER_START");
}

void loop() {
    uint64_t sumSquares = 0; // Use 64-bit to prevent overflow during summation

    // Phase 1: High-Speed Capture
    for (int i = 0; i < SAMPLES; i++) {
        int raw = analogRead(SENSOR_PIN);
        int centered = raw - ADC_OFFSET;
        sumSquares += (int32_t)centered * centered;
        delayMicroseconds(100); // 10kHz sampling rate
    }

    // Phase 2: Scientific Calculation (RMS)
    float rmsRaw = sqrt((float)sumSquares / SAMPLES);
    
    // Convert Raw RMS to Real-World Amperes
    float currentAmps = (rmsRaw / (ADC_RES / 2.0)) * MAX_AMPS;

    // Phase 3: No-Load Filtering (De-noising)
    if (currentAmps < NO_LOAD_CUTOFF) {
        currentAmps = 0.0;
    }

    float powerWatts = currentAmps * VOLTAGE_AC;

    // Phase 4: Terminal Output
    // Format: Amps, Watts (Easy to parse for future Python scripts)
    Serial.printf("DATA | I: %.3f A | P: %.1f W\n", currentAmps, powerWatts);

    delay(500); // Sampling window interval
}