#pragma once

#include <Arduino.h>
#include <TMCStepper.h>

/**
 * @brief Controller class for Trinamic TMC2209 stepper driver.
 * Encapsulates STEP/DIR pulse generation, UART register configuration,
 * microstepping, current control, chopper modes, and diagnostics.
 */
class TMC2209Controller {
public:
    enum MotorMode {
        MODE_STOPPED,
        MODE_CONTINUOUS_FWD,
        MODE_CONTINUOUS_REV,
        MODE_TARGET_STEPS,
        MODE_AUTO_DEMO
    };

    struct Config {
        int8_t stepPin = 25;
        int8_t dirPin = 26;
        int8_t enPin = 27;
        int8_t diagPin = 14;
        int8_t rxPin = 16;
        int8_t txPin = 17;
        HardwareSerial* serialPort = &Serial2;
        float rSense = 0.11f;
        uint8_t driverAddress = 0b00;
        uint16_t fullStepsPerRev = 200;
        uint16_t defaultMicrosteps = 16;
        uint16_t defaultRmsCurrent = 600;      // mA
        uint32_t defaultStepInterval = 800;    // us
        uint32_t minStepInterval = 10;         // us (up to 100,000 steps/s limit)
        uint32_t maxStepInterval = 10000;      // us
        uint8_t defaultSgThreshold = 64;       // StallGuard threshold (0-255)
    };

    TMC2209Controller();
    ~TMC2209Controller();

    /**
     * @brief Initializes GPIO pins, UART bus, and TMC2209 registers.
     * @param config Hardware pinout and default driver parameters.
     * @return true if UART communication is confirmed, false otherwise.
     */
    bool begin();
    bool begin(const Config& config);

    /**
     * @brief Periodic motion handler to be called inside Arduino loop().
     * Generates non-blocking step pulses for continuous, target, and demo modes.
     */
    void update();

    // ---------------- Motion Control ----------------
    void moveForward();
    void moveReverse();
    void moveSteps(uint32_t steps, bool forward = true);
    void moveRevolutions(float revolutions, bool forward = true);
    void stop();
    void toggleAutoDemo();

    // ---------------- Speed & RPM Control ----------------
    void setStepInterval(uint32_t intervalUs);
    uint32_t getStepInterval() const { return _stepIntervalMicros; }
    float getStepsPerSecond() const;
    float getRPM() const;
    void setRPM(float rpm);
    void increaseSpeed(uint32_t deltaUs = 50);
    void decreaseSpeed(uint32_t deltaUs = 50);

    // ---------------- Driver Configuration ----------------
    void setEnable(bool enable);
    void toggleEnable();
    bool isEnabled() const { return _driverEnabled; }

    void setDirection(bool forward);
    bool getDirection() const { return _directionForward; }

    void setMicrosteps(uint16_t ms);
    uint16_t getMicrosteps() const { return _currentMicrosteps; }
    void cycleMicrosteps();

    void setRmsCurrent(uint16_t mA);
    uint16_t getRmsCurrent() const { return _currentRmsCurrent; }
    void cycleRmsCurrent();

    void setSpreadCycle(bool enable);
    void toggleSpreadCycle();
    bool isSpreadCycle() const;

    // ---------------- Status & Diagnostics ----------------
    struct DiagTelemetry {
        bool ot = false;
        bool otpw = false;
        bool stst = true;
        uint8_t cs = 0;
        uint16_t sg = 0;
        uint8_t version = 0x21;
        bool uartOk = true;
        bool stall = false;
    };

    uint8_t testConnection();
    uint8_t getSiliconVersion();
    DiagTelemetry getCachedDiag();
    void printDiagnostics(Print& out = Serial);

    // ---------------- StallGuard & Lost Step Detection ----------------
    uint16_t getStallGuardResult();
    bool isStallDetected();
    void setStallThreshold(uint8_t threshold);
    uint8_t getStallThreshold() const { return _sgThreshold; }

    MotorMode getMode() const { return _currentMode; }
    bool isAutoDemo() const { return _currentMode == MODE_AUTO_DEMO; }
    TMC2209Stepper* getDriver() { return _driver; }
    const Config& getConfig() const { return _config; }

private:
    void triggerStepPulse();
    uint32_t getStartInterval() const;

    Config _config;
    TMC2209Stepper* _driver = nullptr;

    volatile MotorMode _currentMode = MODE_STOPPED;
    volatile bool _driverEnabled = true;
    volatile bool _directionForward = true;

    volatile uint16_t _currentMicrosteps = 16;
    volatile uint16_t _currentRmsCurrent = 600;
    volatile uint32_t _stepIntervalMicros = 800;
    uint32_t _activeIntervalMicros = 1000; // Acceleration ramp interval
    uint8_t _sgThreshold = 64;

    uint32_t _stepCounter = 0;
    uint32_t _targetSteps = 0;
    unsigned long _lastStepTime = 0;

    DiagTelemetry _cachedDiag;
    unsigned long _lastDiagTime = 0;

    // Auto-demo state variables
    enum DemoSubState { DEMO_FWD, DEMO_PAUSE_1, DEMO_REV, DEMO_PAUSE_2 };
    DemoSubState _demoState = DEMO_FWD;
    unsigned long _demoStateStartTime = 0;
    static const unsigned long DEMO_PAUSE_MS = 600;
};
