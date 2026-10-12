#include "INA226Monitor.h"

INA226Monitor::INA226Monitor() {
}

INA226Monitor::~INA226Monitor() {
    if (_sensor) {
        delete _sensor;
        _sensor = nullptr;
    }
}

bool INA226Monitor::begin() {
    return begin(Config());
}

bool INA226Monitor::begin(const Config& config) {
    _config = config;

    // Initialize I2C bus with specified SDA/SCL pins and safe hardware timeout
    if (_config.wire) {
        _config.wire->begin(_config.sdaPin, _config.sclPin);
        _config.wire->setTimeOut(25); // 25ms timeout prevents hangs if motor noise glitches bus
    }

    if (_sensor) {
        delete _sensor;
    }
    _sensor = new INA226(_config.i2cAddress, _config.wire);

    Serial.println(F("[INIT] Initializing INA226 Power Monitor..."));
    bool ok = _sensor->begin();

    if (!ok || !_sensor->isConnected()) {
        Serial.printf("[WARN] INA226 not detected on I2C address 0x%02X! Check SDA/SCL wiring & pullups.\n",
                      _config.i2cAddress);
        _initialized = false;
        return false;
    }

    // Configure shunt calibration (R1 = 0.1 Ohm, maxCurrent = 0.8A)
    int calStatus = _sensor->setMaxCurrentShunt(_config.maxCurrent, _config.shuntResistance);
    if (calStatus != INA226_ERR_NONE) {
        Serial.printf("[WARN] INA226 calibration note (code 0x%04X)\n", calStatus);
    }

    // Configure hardware averaging and conversion time for smooth motor current readings
    _sensor->setAverage(INA226_16_SAMPLES);
    _sensor->setBusVoltageConversionTime(INA226_1100_us);
    _sensor->setShuntVoltageConversionTime(INA226_1100_us);
    _sensor->setModeShuntBusContinuous();

    _initialized = true;
    Serial.println(F("[INIT] SUCCESS: INA226 communication established!"));
    Serial.printf("[INIT] INA226 Address: 0x%02X | Shunt: %.2f Ohm | Max Current: %.2f A\n",
                  _config.i2cAddress, _config.shuntResistance, _config.maxCurrent);

    return true;
}

bool INA226Monitor::isConnected() {
    return _initialized && _sensor && _sensor->isConnected();
}

INA226Monitor::Telemetry INA226Monitor::read() {
    unsigned long now = millis();
    if (_cachedTelemetry.valid && (now - _lastReadTime < 250)) {
        return _cachedTelemetry;
    }
    _lastReadTime = now;

    Telemetry t;
    if (!_initialized || !_sensor || !_sensor->isConnected()) {
        t.valid = false;
        _cachedTelemetry = t;
        return t;
    }

    t.busVoltage_V = _sensor->getBusVoltage();
    t.shuntVoltage_mV = _sensor->getShuntVoltage_mV();

    // Calculate current from shunt voltage drop (I = Vshunt / Rshunt)
    // and cross-check with sensor hardware current register
    if (_config.shuntResistance > 0.0f) {
        t.current_mA = (t.shuntVoltage_mV / _config.shuntResistance);
    } else {
        t.current_mA = _sensor->getCurrent_mA();
    }

    // Power (W) = Vbus (V) * Current (A)
    t.power_W = (t.busVoltage_V * (t.current_mA / 1000.0f));
    t.valid = true;
    _cachedTelemetry = t;
    return t;
}

float INA226Monitor::getBusVoltage() {
    return read().busVoltage_V;
}

float INA226Monitor::getShuntVoltage_mV() {
    return read().shuntVoltage_mV;
}

float INA226Monitor::getCurrent_mA() {
    return read().current_mA;
}

float INA226Monitor::getPower_W() {
    return read().power_W;
}

void INA226Monitor::printMetrics(Print& out) {
    out.println(F("-------------------------------------------"));
    out.println(F("       INA226 POWER & VOLTAGE MONITOR      "));
    out.println(F("-------------------------------------------"));

    if (!_initialized || !_sensor || !_sensor->isConnected()) {
        out.println(F("I2C Connection       : NOT CONNECTED / ERROR"));
        out.printf("Target Address       : 0x%02X (Check 3.3V, GND, SDA=21, SCL=22)\n", _config.i2cAddress);
        out.println(F("-------------------------------------------\n"));
        return;
    }

    Telemetry t = read();
    out.printf("I2C Connection       : OK (Address 0x%02X)\n", _config.i2cAddress);
    out.printf("Bus Voltage (VBUS)   : %.2f V\n", t.busVoltage_V);
    out.printf("Shunt Voltage (VSHUNT: %.2f mV\n", t.shuntVoltage_mV);
    out.printf("Load Current         : %.1f mA\n", t.current_mA);
    out.printf("Power Consumption    : %.2f W\n", t.power_W);
    out.println(F("-------------------------------------------\n"));
}
