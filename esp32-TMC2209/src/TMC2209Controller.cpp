#include "TMC2209Controller.h"

TMC2209Controller::TMC2209Controller() {
}

TMC2209Controller::~TMC2209Controller() {
    if (_driver) {
        delete _driver;
        _driver = nullptr;
    }
}

bool TMC2209Controller::begin() {
    return begin(Config());
}

bool TMC2209Controller::begin(const Config& config) {
    _config = config;
    _currentMicrosteps = _config.defaultMicrosteps;
    _currentRmsCurrent = _config.defaultRmsCurrent;
    _stepIntervalMicros = _config.defaultStepInterval;

    // Configure hardware GPIO pins
    pinMode(_config.enPin, OUTPUT);
    pinMode(_config.dirPin, OUTPUT);
    pinMode(_config.stepPin, OUTPUT);
    if (_config.diagPin >= 0) {
        pinMode(_config.diagPin, INPUT_PULLDOWN);
    }

    digitalWrite(_config.stepPin, LOW);
    setDirection(true);
    setEnable(true);

    // Initialize UART communication with TMC2209
    if (_config.serialPort) {
        _config.serialPort->begin(115200, SERIAL_8N1, _config.rxPin, _config.txPin);
    }

    if (_driver) {
        delete _driver;
    }
    _driver = new TMC2209Stepper(_config.serialPort, _config.rSense, _config.driverAddress);

    Serial.println(F("[INIT] Initializing TMC2209 via UART..."));
    _driver->begin();

    // Verify driver connection
    uint8_t connResult = testConnection();
    if (connResult == 0) {
        Serial.println(F("[INIT] SUCCESS: TMC2209 UART communication established!"));
    } else if (connResult == 1) {
        Serial.println(F("[WARN] Driver responded with 0xFFFFFFFF (Check wiring / 1k resistor)."));
    } else {
        Serial.println(F("[WARN] No response from TMC2209 (Check VMOT power, VIO 3.3V, address)."));
    }

    uint8_t version = getSiliconVersion();
    _cachedDiag.version = version;
    _cachedDiag.uartOk = (connResult == 0);
    Serial.printf("[INIT] TMC2209 Silicon Version: 0x%02X\n", version);

    // Apply driver default registers
    _sgThreshold = _config.defaultSgThreshold;
    _driver->toff(5);                        // Enable driver internal stage (toff > 0)
    _driver->blank_time(24);
    _driver->rms_current(_currentRmsCurrent); // RMS current in mA
    _driver->microsteps(_currentMicrosteps);  // Microsteps: 16
    _driver->pwm_autoscale(true);            // Required for StealthChop voltage PWM
    _driver->en_spreadCycle(false);          // False = StealthChop (ultra-silent)
    _driver->TCOOLTHRS(0xFFFFF);             // Enable StallGuard4 across all operating speeds
    _driver->SGTHRS(_sgThreshold);           // StallGuard threshold (0-255)

    Serial.printf("[INIT] Current: %u mA | Microsteps: 1/%u | StealthChop: ON | SGTHRS: %u\n",
                  _currentRmsCurrent, _currentMicrosteps, _sgThreshold);

    return (connResult == 0);
}

void TMC2209Controller::triggerStepPulse() {
    digitalWrite(_config.stepPin, HIGH);
    delayMicroseconds(3); // TMC2209 minimum pulse width is 100ns
    digitalWrite(_config.stepPin, LOW);
}

uint32_t TMC2209Controller::getStartInterval() const {
    float totalSteps = (float)_config.fullStepsPerRev * _currentMicrosteps;
    if (totalSteps <= 0.0f) return _stepIntervalMicros;
    // Start gently at ~30 RPM (or target interval if target is even slower)
    uint32_t startUs = (uint32_t)((60.0f * 1000000.0f) / (30.0f * totalSteps));
    return max(startUs, _stepIntervalMicros);
}

void TMC2209Controller::update() {
    if (!_driverEnabled || _currentMode == MODE_STOPPED) {
        return;
    }

    unsigned long nowMicros = micros();

    // Dynamic acceleration/deceleration ramp
    auto advanceRamp = [this]() {
        if (_activeIntervalMicros > _stepIntervalMicros) {
            uint32_t diff = _activeIntervalMicros - _stepIntervalMicros;
            uint32_t delta = (diff > 5000) ? 80 : ((diff > 1500) ? 30 : ((diff > 300) ? 8 : 2));
            if (_activeIntervalMicros > _stepIntervalMicros + delta) {
                _activeIntervalMicros -= delta;
            } else {
                _activeIntervalMicros = _stepIntervalMicros;
            }
        } else if (_activeIntervalMicros < _stepIntervalMicros) {
            uint32_t diff = _stepIntervalMicros - _activeIntervalMicros;
            uint32_t delta = (diff > 5000) ? 100 : ((diff > 1500) ? 40 : ((diff > 300) ? 10 : 2));
            if (_activeIntervalMicros + delta < _stepIntervalMicros) {
                _activeIntervalMicros += delta;
            } else {
                _activeIntervalMicros = _stepIntervalMicros;
            }
        }
    };

    // Auto Demo Mode
    if (_currentMode == MODE_AUTO_DEMO) {
        unsigned long nowMillis = millis();
        uint32_t oneRevolutionSteps = (uint32_t)_config.fullStepsPerRev * _currentMicrosteps;

        switch (_demoState) {
            case DEMO_FWD:
                setDirection(true);
                if (nowMicros - _lastStepTime >= _activeIntervalMicros) {
                    _lastStepTime = nowMicros;
                    triggerStepPulse();
                    advanceRamp();
                    _stepCounter++;
                    if (_stepCounter >= oneRevolutionSteps) {
                        _stepCounter = 0;
                        _demoState = DEMO_PAUSE_1;
                        _demoStateStartTime = nowMillis;
                        _activeIntervalMicros = getStartInterval();
                    }
                }
                break;

            case DEMO_PAUSE_1:
                if (nowMillis - _demoStateStartTime >= DEMO_PAUSE_MS) {
                    _demoState = DEMO_REV;
                    _lastStepTime = nowMicros;
                    _activeIntervalMicros = getStartInterval();
                }
                break;

            case DEMO_REV:
                setDirection(false);
                if (nowMicros - _lastStepTime >= _activeIntervalMicros) {
                    _lastStepTime = nowMicros;
                    triggerStepPulse();
                    advanceRamp();
                    _stepCounter++;
                    if (_stepCounter >= oneRevolutionSteps) {
                        _stepCounter = 0;
                        _demoState = DEMO_PAUSE_2;
                        _demoStateStartTime = nowMillis;
                        _activeIntervalMicros = getStartInterval();
                    }
                }
                break;

            case DEMO_PAUSE_2:
                if (nowMillis - _demoStateStartTime >= DEMO_PAUSE_MS) {
                    _demoState = DEMO_FWD;
                    _lastStepTime = nowMicros;
                    _activeIntervalMicros = getStartInterval();
                }
                break;
        }
        return;
    }

    // Target Steps Mode
    if (_currentMode == MODE_TARGET_STEPS) {
        if (nowMicros - _lastStepTime >= _activeIntervalMicros) {
            _lastStepTime = nowMicros;
            triggerStepPulse();
            advanceRamp();
            _stepCounter++;
            if (_stepCounter >= _targetSteps) {
                _currentMode = MODE_STOPPED;
                _stepCounter = 0;
                _targetSteps = 0;
                Serial.println(F("[MOTOR] Target steps completed."));
            }
        }
        return;
    }

    // Continuous Modes
    if (_currentMode == MODE_CONTINUOUS_FWD || _currentMode == MODE_CONTINUOUS_REV) {
        if (nowMicros - _lastStepTime >= _activeIntervalMicros) {
            _lastStepTime = nowMicros;
            triggerStepPulse();
            advanceRamp();
            _stepCounter++;
        }
    }
}

// ---------------- Motion Control ----------------
void TMC2209Controller::moveForward() {
    setDirection(true);
    _currentMode = MODE_CONTINUOUS_FWD;
    _lastStepTime = micros();
    _activeIntervalMicros = getStartInterval();
    Serial.printf("[CMD] Continuous FORWARD running (Interval: %u us, ~%.1f RPM)...\n",
                  _stepIntervalMicros, getRPM());
}

void TMC2209Controller::moveReverse() {
    setDirection(false);
    _currentMode = MODE_CONTINUOUS_REV;
    _lastStepTime = micros();
    _activeIntervalMicros = getStartInterval();
    Serial.printf("[CMD] Continuous REVERSE running (Interval: %u us, ~%.1f RPM)...\n",
                  _stepIntervalMicros, getRPM());
}

void TMC2209Controller::moveSteps(uint32_t steps, bool forward) {
    setDirection(forward);
    _stepCounter = 0;
    _targetSteps = steps;
    _currentMode = MODE_TARGET_STEPS;
    _lastStepTime = micros();
    _activeIntervalMicros = getStartInterval();
    Serial.printf("[CMD] Moving %u steps %s (~%.1f RPM)...\n",
                  steps, forward ? "FORWARD" : "REVERSE", getRPM());
}

void TMC2209Controller::moveRevolutions(float revolutions, bool forward) {
    uint32_t steps = (uint32_t)((float)_config.fullStepsPerRev * _currentMicrosteps * revolutions);
    moveSteps(steps, forward);
}

void TMC2209Controller::stop() {
    _currentMode = MODE_STOPPED;
    _stepCounter = 0;
    _targetSteps = 0;
    Serial.println(F("[CMD] Motor STOPPED."));
}

void TMC2209Controller::toggleAutoDemo() {
    if (_currentMode == MODE_AUTO_DEMO) {
        _currentMode = MODE_STOPPED;
        Serial.println(F("[CMD] Auto-Demo STOPPED."));
    } else {
        _currentMode = MODE_AUTO_DEMO;
        _demoState = DEMO_FWD;
        _stepCounter = 0;
        _lastStepTime = micros();
        _activeIntervalMicros = getStartInterval();
        Serial.println(F("[CMD] Auto-Demo STARTED (alternating CW/CCW 1 rev)."));
    }
}

// ---------------- Speed & RPM Control ----------------
void TMC2209Controller::setStepInterval(uint32_t intervalUs) {
    if (intervalUs < _config.minStepInterval) {
        intervalUs = _config.minStepInterval;
    } else if (intervalUs > _config.maxStepInterval) {
        intervalUs = _config.maxStepInterval;
    }

    // Allow testing up to 2500 RPM for step-loss threshold benchmarking
    float totalSteps = (float)_config.fullStepsPerRev * _currentMicrosteps;
    if (totalSteps > 0.0f) {
        float calcRpm = ((1000000.0f / (float)intervalUs) * 60.0f) / totalSteps;
        const float MAX_TEST_RPM = 2500.0f;
        if (calcRpm > MAX_TEST_RPM) {
            intervalUs = (uint32_t)((60.0f * 1000000.0f) / (MAX_TEST_RPM * totalSteps));
            if (intervalUs < _config.minStepInterval) intervalUs = _config.minStepInterval;
        }
    }

    _stepIntervalMicros = intervalUs;
    Serial.printf("[SPEED] Interval = %u us (~%.1f steps/s, ~%.1f RPM)\n",
                  _stepIntervalMicros, getStepsPerSecond(), getRPM());
}

float TMC2209Controller::getStepsPerSecond() const {
    return _stepIntervalMicros > 0 ? (1000000.0f / (float)_stepIntervalMicros) : 0.0f;
}

float TMC2209Controller::getRPM() const {
    float totalSteps = (float)_config.fullStepsPerRev * _currentMicrosteps;
    return totalSteps > 0 ? (getStepsPerSecond() * 60.0f / totalSteps) : 0.0f;
}

void TMC2209Controller::setRPM(float rpm) {
    if (rpm <= 0.1f) rpm = 0.1f;
    if (rpm > 2500.0f) rpm = 2500.0f; // Allow testing up to 2500 RPM to find step-loss threshold
    float totalSteps = (float)_config.fullStepsPerRev * _currentMicrosteps;
    float stepsPerSec = (rpm * totalSteps) / 60.0f;
    if (stepsPerSec > 0.0f) {
        setStepInterval((uint32_t)(1000000.0f / stepsPerSec));
    }
}

void TMC2209Controller::increaseSpeed(uint32_t deltaUs) {
    if (_stepIntervalMicros > _config.minStepInterval + deltaUs) {
        setStepInterval(_stepIntervalMicros - deltaUs);
    } else {
        setStepInterval(_config.minStepInterval);
    }
}

void TMC2209Controller::decreaseSpeed(uint32_t deltaUs) {
    if (_stepIntervalMicros + deltaUs <= _config.maxStepInterval) {
        setStepInterval(_stepIntervalMicros + deltaUs);
    } else {
        setStepInterval(_config.maxStepInterval);
    }
}

// ---------------- Driver Configuration ----------------
void TMC2209Controller::setEnable(bool enable) {
    _driverEnabled = enable;
    digitalWrite(_config.enPin, enable ? LOW : HIGH); // LOW enables TMC2209
}

void TMC2209Controller::toggleEnable() {
    setEnable(!_driverEnabled);
    Serial.printf("[CMD] Driver output %s (EN pin %s)\n",
                  _driverEnabled ? "ENABLED" : "DISABLED",
                  _driverEnabled ? "LOW" : "HIGH");
}

void TMC2209Controller::setDirection(bool forward) {
    _directionForward = forward;
    digitalWrite(_config.dirPin, forward ? HIGH : LOW);
}

void TMC2209Controller::setMicrosteps(uint16_t ms) {
    float targetRpm = getRPM();
    if (targetRpm < 1.0f) targetRpm = 60.0f; // Default sensible fallback

    _currentMicrosteps = ms;
    if (_driver) {
        _driver->microsteps(_currentMicrosteps);
    }
    // Maintain RPM smoothly across microstep mode changes
    setRPM(targetRpm);

    Serial.printf("[CONFIG] Microstepping set to 1/%u (Target: ~%.1f RPM, Interval: %u us)\n",
                  _currentMicrosteps, getRPM(), _stepIntervalMicros);
}

void TMC2209Controller::cycleMicrosteps() {
    static const uint16_t msList[] = {1, 2, 4, 8, 16, 32, 64, 128, 256};
    static uint8_t msIdx = 4; // default 16 is at index 4
    msIdx = (msIdx + 1) % 9;
    setMicrosteps(msList[msIdx]);
}

void TMC2209Controller::setRmsCurrent(uint16_t mA) {
    _currentRmsCurrent = mA;
    if (_driver) {
        _driver->rms_current(_currentRmsCurrent);
    }
    Serial.printf("[CONFIG] RMS Current set to %u mA\n", _currentRmsCurrent);
}

void TMC2209Controller::cycleRmsCurrent() {
    static const uint16_t currList[] = {300, 500, 700, 900, 1100};
    static uint8_t currIdx = 2; // default 700 is at index 2
    currIdx = (currIdx + 1) % 5;
    setRmsCurrent(currList[currIdx]);
}

void TMC2209Controller::setSpreadCycle(bool enable) {
    if (_driver) {
        _driver->en_spreadCycle(enable);
    }
    Serial.printf("[MODE] Switched to %s mode\n",
                  enable ? "SpreadCycle (Torque)" : "StealthChop (Quiet)");
}

void TMC2209Controller::toggleSpreadCycle() {
    setSpreadCycle(!isSpreadCycle());
}

bool TMC2209Controller::isSpreadCycle() const {
    return _driver ? _driver->en_spreadCycle() : false;
}

// ---------------- Status & Diagnostics ----------------
uint8_t TMC2209Controller::testConnection() {
    return _driver ? _driver->test_connection() : 2;
}

uint8_t TMC2209Controller::getSiliconVersion() {
    return _driver ? _driver->version() : 0;
}

void TMC2209Controller::printDiagnostics(Print& out) {
    out.println(F("\n-------------------------------------------"));
    out.println(F("       TMC2209 DRIVER DIAGNOSTICS          "));
    out.println(F("-------------------------------------------"));

    uint8_t conn = testConnection();
    out.printf("UART Connection      : %s (code %u)\n",
               conn == 0 ? "OK" : (conn == 1 ? "HIGH IMPEDANCE (0xFFFFFFFF)" : "NO RESPONSE (0x00)"),
               conn);

    uint8_t version = getSiliconVersion();
    out.printf("Silicon Version      : 0x%02X %s\n",
               version,
               (version == 0x21 ? "[TMC2209 VALID]" : "[UNKNOWN/DISCONNECTED]"));

    out.printf("Driver Output (EN)   : %s\n", _driverEnabled ? "ENABLED (EN=LOW)" : "DISABLED (EN=HIGH)");
    out.printf("RMS Motor Current    : %u mA\n", _driver ? _driver->rms_current() : _currentRmsCurrent);
    out.printf("Microstep Setting    : 1/%u\n", _driver ? _driver->microsteps() : _currentMicrosteps);
    out.printf("Chopper Mode         : %s\n", isSpreadCycle() ? "SpreadCycle (Torque)" : "StealthChop (Quiet)");

    if (_driver) {
        uint32_t drvStatus = _driver->DRV_STATUS();
        out.printf("DRV_STATUS Register  : 0x%08X\n", drvStatus);
        out.printf(" - Overtemp Prewarn  : %s\n", _driver->otpw() ? "WARNING (Hot)" : "Normal");
        out.printf(" - Overtemp Shutdown : %s\n", _driver->ot() ? "FAULT (Thermal Shutdown)" : "Normal");
        out.printf(" - Short to GND CoilA: %s\n", _driver->s2ga() ? "FAULT" : "OK");
        out.printf(" - Short to GND CoilB: %s\n", _driver->s2gb() ? "FAULT" : "OK");
        out.printf(" - Open Load Coil A  : %s\n", _driver->ola() ? "OPEN LOAD" : "OK");
        out.printf(" - Open Load Coil B  : %s\n", _driver->olb() ? "OPEN LOAD" : "OK");
        out.printf(" - Motor Standstill  : %s\n", _driver->stst() ? "YES" : "NO");
        out.printf(" - Current Scale (CS): %u / 31\n", _driver->cs_actual());
        out.printf(" - StallGuard Result : %u\n", _driver->SG_RESULT());
    }

    if (_config.diagPin >= 0) {
        out.printf(" - DIAG Pin (GPIO%d)  : %s\n", _config.diagPin,
                   digitalRead(_config.diagPin) ? "HIGH (Stall / Trigger)" : "LOW");
    }
    out.println(F("-------------------------------------------\n"));
}

// ---------------- StallGuard & Lost Step Detection ----------------
TMC2209Controller::DiagTelemetry TMC2209Controller::getCachedDiag() {
    unsigned long now = millis();
    if (_lastDiagTime > 0 && (now - _lastDiagTime < 250)) {
        return _cachedDiag;
    }
    _lastDiagTime = now;

    if (_driver) {
        uint32_t drvStatus = _driver->DRV_STATUS();
        _cachedDiag.ot = (drvStatus & (1UL << 1));
        _cachedDiag.otpw = (drvStatus & (1UL << 0));
        _cachedDiag.stst = (drvStatus & (1UL << 31));
        _cachedDiag.cs = (drvStatus >> 16) & 0x1F;
        _cachedDiag.sg = _driver->SG_RESULT();
        _cachedDiag.stall = (_currentMode != MODE_STOPPED && !_cachedDiag.stst && !isSpreadCycle() && _cachedDiag.sg == 0);
    }
    return _cachedDiag;
}

uint16_t TMC2209Controller::getStallGuardResult() {
    return getCachedDiag().sg;
}

bool TMC2209Controller::isStallDetected() {
    return getCachedDiag().stall;
}

void TMC2209Controller::setStallThreshold(uint8_t threshold) {
    _sgThreshold = threshold;
    if (_driver) {
        _driver->SGTHRS(_sgThreshold);
    }
    Serial.printf("[CONFIG] StallGuard Threshold set to %u\n", _sgThreshold);
}
