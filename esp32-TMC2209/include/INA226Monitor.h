#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <INA226.h>

/**
 * @brief Monitor class for Texas Instruments INA226 current/power sensor.
 * Measures high-side bus voltage, shunt voltage drop, load current,
 * and power consumption of the stepper motor driver supply.
 */
class INA226Monitor {
public:
    struct Config {
        uint8_t i2cAddress = 0x40;    // Default address (A0=GND, A1=GND)
        int8_t sdaPin = 21;           // ESP32 default I2C SDA
        int8_t sclPin = 22;           // ESP32 default I2C SCL
        float shuntResistance = 0.1f; // Onboard R1 = 0.1 Ohm (100 mOhm)
        float maxCurrent = 0.8f;      // Max current (80mV / 0.1 Ohm = 0.8A)
        TwoWire* wire = &Wire;
    };

    struct Telemetry {
        float busVoltage_V = 0.0f;
        float shuntVoltage_mV = 0.0f;
        float current_mA = 0.0f;
        float power_W = 0.0f;
        bool valid = false;
    };

    INA226Monitor();
    ~INA226Monitor();

    bool begin();
    bool begin(const Config& config);

    bool isConnected();

    Telemetry read();

    float getBusVoltage();
    float getShuntVoltage_mV();
    float getCurrent_mA();
    float getPower_W();

    void printMetrics(Print& out = Serial);

    INA226* getRawSensor() { return _sensor; }
    const Config& getConfig() const { return _config; }

private:
    Config _config;
    INA226* _sensor = nullptr;
    bool _initialized = false;
    Telemetry _cachedTelemetry;
    unsigned long _lastReadTime = 0;
};
