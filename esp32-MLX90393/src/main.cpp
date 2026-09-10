#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MLX90393.h>
#include <math.h>

Adafruit_MLX90393 sensor = Adafruit_MLX90393();

// ===================== TUNING =====================
const float STABILITY_THRESHOLD = 0.50;  // max allowed variation to be considered still
const int   STABILITY_SAMPLES   = 15;    // number of recent samples to check
const float DEADZONE            = 0.40;  // force very small values to zero
const float PRINT_INTERVAL      = 120;

// ===================== Calibration =====================
float offsetX = 0, offsetY = 0, offsetZ = 0;
float scaleX  = 1, scaleY  = 1, scaleZ  = 1;

float zeroX = 0, zeroY = 0, zeroZ = 0;
bool  zeroSet = false;

// ===================== Stability =====================
float histX[STABILITY_SAMPLES];
float histY[STABILITY_SAMPLES];
float histZ[STABILITY_SAMPLES];
int   histIndex = 0;
bool  histFilled = false;

float heldX = 0, heldY = 0, heldZ = 0;
bool  isStable = false;

float applyDeadzone(float v) {
    return (fabs(v) < DEADZONE) ? 0.0f : v;
}

bool checkStability(float x, float y, float z)
{
    histX[histIndex] = x;
    histY[histIndex] = y;
    histZ[histIndex] = z;
    histIndex = (histIndex + 1) % STABILITY_SAMPLES;
    if (histIndex == 0) histFilled = true;

    if (!histFilled) return false;

    float minX = histX[0], maxX = histX[0];
    float minY = histY[0], maxY = histY[0];
    float minZ = histZ[0], maxZ = histZ[0];

    for (int i = 1; i < STABILITY_SAMPLES; i++) {
        if (histX[i] < minX) minX = histX[i];
        if (histX[i] > maxX) maxX = histX[i];
        if (histY[i] < minY) minY = histY[i];
        if (histY[i] > maxY) maxY = histY[i];
        if (histZ[i] < minZ) minZ = histZ[i];
        if (histZ[i] > maxZ) maxZ = histZ[i];
    }

    float rangeX = maxX - minX;
    float rangeY = maxY - minY;
    float rangeZ = maxZ - minZ;

    return (rangeX < STABILITY_THRESHOLD &&
            rangeY < STABILITY_THRESHOLD &&
            rangeZ < STABILITY_THRESHOLD);
}

void runHardCalibration(unsigned long duration_ms = 18000)
{
    Serial.println("\n=== HARD/SOFT-IRON CALIBRATION ===");
    Serial.println("Rotate slowly in all directions...");

    float minX = 1e6, maxX = -1e6;
    float minY = 1e6, maxY = -1e6;
    float minZ = 1e6, maxZ = -1e6;

    unsigned long start = millis();
    while (millis() - start < duration_ms) {
        float x, y, z;
        if (sensor.readData(&x, &y, &z)) {
            if (x < minX) minX = x; if (x > maxX) maxX = x;
            if (y < minY) minY = y; if (y > maxY) maxY = y;
            if (z < minZ) minZ = z; if (z > maxZ) maxZ = z;
        }
        delay(15);
    }

    offsetX = (minX + maxX) / 2.0f;
    offsetY = (minY + maxY) / 2.0f;
    offsetZ = (minZ + maxZ) / 2.0f;

    float rangeX = max(maxX - minX, 1.0f);
    float rangeY = max(maxY - minY, 1.0f);
    float rangeZ = max(maxZ - minZ, 1.0f);
    float avgRange = (rangeX + rangeY + rangeZ) / 3.0f;

    scaleX = avgRange / rangeX;
    scaleY = avgRange / rangeY;
    scaleZ = avgRange / rangeZ;

    zeroSet = false;
    histFilled = false;
    isStable = false;

    Serial.println("Calibration done. Place in ZERO position and send 'z'\n");
}

void setZeroPosition()
{
    float sumX = 0, sumY = 0, sumZ = 0;
    for (int i = 0; i < 40; i++) {
        float x, y, z;
        if (sensor.readData(&x, &y, &z)) {
            sumX += (x - offsetX) * scaleX;
            sumY += (y - offsetY) * scaleY;
            sumZ += (z - offsetZ) * scaleZ;
        }
        delay(25);
    }

    zeroX = sumX / 40.0f;
    zeroY = sumY / 40.0f;
    zeroZ = sumZ / 40.0f;
    zeroSet = true;

    histFilled = false;
    isStable = false;

    Serial.println("\n=== ZERO SET ===\n");
}

void setup()
{
    Serial.begin(115200);
    while (!Serial) delay(10);

    Wire.begin(21, 22);

    if (!sensor.begin_I2C()) {
        Serial.println("MLX90393 not found!");
        while (1) delay(10);
    }
    Serial.println("MLX90393 found");

    sensor.setGain(MLX90393_GAIN_1X);
    sensor.setResolution(MLX90393_X, MLX90393_RES_17);
    sensor.setResolution(MLX90393_Y, MLX90393_RES_17);
    sensor.setResolution(MLX90393_Z, MLX90393_RES_16);
    sensor.setOversampling(MLX90393_OSR_3);
    sensor.setFilter(MLX90393_FILTER_7);

    runHardCalibration(18000);
}

void loop()
{
    static unsigned long lastPrint = 0;

    float rawX, rawY, rawZ;
    if (!sensor.readData(&rawX, &rawY, &rawZ)) {
        delay(8);
        return;
    }

    // Apply calibration + zero
    float x = (rawX - offsetX) * scaleX - zeroX;
    float y = (rawY - offsetY) * scaleY - zeroY;
    float z = (rawZ - offsetZ) * scaleZ - zeroZ;

    // Check if currently stable
    bool currentlyStable = checkStability(x, y, z);

    float outX, outY, outZ;

    if (currentlyStable) {
        if (!isStable) {
            // Just became stable → freeze current values
            heldX = applyDeadzone(x);
            heldY = applyDeadzone(y);
            heldZ = applyDeadzone(z);
            isStable = true;
        }
        outX = heldX;
        outY = heldY;
        outZ = heldZ;
    } else {
        // Moving → show live values
        isStable = false;
        outX = applyDeadzone(x);
        outY = applyDeadzone(y);
        outZ = applyDeadzone(z);
    }

    // Print
    if (millis() - lastPrint >= PRINT_INTERVAL) {
        lastPrint = millis();
        float mag = sqrt(outX*outX + outY*outY + outZ*outZ);

        Serial.printf("X:%7.2f  Y:%7.2f  Z:%7.2f  |mag|:%6.2f %s",
                      outX, outY, outZ, mag,
                      isStable ? "[STABLE]" : "");
        if (!zeroSet) Serial.print(" [zero not set]");
        Serial.println();
    }

    // Commands
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 'c' || c == 'C') runHardCalibration(18000);
        if (c == 'z' || c == 'Z') setZeroPosition();
    }
}