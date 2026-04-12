#include <iostream>
#include <vector>
#include <cmath>

int main() {
    const int SAMPLES = 1000;
    const int ADC_OFFSET = 2048;
    const float MAX_AMPS = 30.0;
    const float simCurrentRMS = 15.0;

    long long sumSquares = 0;

    std::cout << "Starting Logic Test..." << std::endl;

    for (int i = 0; i < SAMPLES; i++) {
        // Simulation logic
        float t = (i * 100.0) / 1000000.0; // Simulate 100us steps
        float peakADC = (simCurrentRMS * sqrt(2) / MAX_AMPS) * 2048.0;
        float virtualSignal = 2048.0 + (peakADC * sin(2.0 * M_PI * 60.0 * t));
        
        int raw = (int)virtualSignal;

        // The actual code logic you wrote:
        int centered = raw - ADC_OFFSET;
        sumSquares += (long long)centered * centered;
    }

    float rmsRaw = sqrt((float)sumSquares / SAMPLES);
    float amps = (rmsRaw / 2048.0) * MAX_AMPS;

    std::cout << "---------------------------" << std::endl;
    std::cout << "Expected: " << simCurrentRMS << " A" << std::endl;
    std::cout << "Calculated: " << amps << " A" << std::endl;
    std::cout << "---------------------------" << std::endl;

    return 0;
}