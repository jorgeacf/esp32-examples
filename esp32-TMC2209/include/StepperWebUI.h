#pragma once

#include <Arduino.h>
#include <WebServer.h>

class TMC2209Controller;
class INA226Monitor;
class WiFiService;

/**
 * @brief Web-based User Interface and REST API service for ESP32.
 * Provides a modern, responsive web dashboard to control the TMC2209 stepper
 * motor and monitor live INA226 voltage, current, and power telemetry.
 */
class StepperWebUI {
public:
    struct Config {
        uint16_t port = 80;
    };

    StepperWebUI(TMC2209Controller& stepper, INA226Monitor& powerMonitor, WiFiService& wifiService);
    ~StepperWebUI();

    bool begin();
    bool begin(const Config& config);

    /**
     * @brief Polls incoming HTTP client requests. Called in main loop().
     */
    void update();

    uint16_t getPort() const { return _config.port; }
    bool isRunning() const { return _running; }

private:
    void handleRoot();
    void handleApiStatus();
    void handleApiControl();
    void handleNotFound();

    Config _config;
    TMC2209Controller& _stepper;
    INA226Monitor& _powerMonitor;
    WiFiService& _wifiService;
    WebServer _server;
    bool _running = false;
};
