#include <Arduino.h>
#include <TMCStepper.h>

/**
 * ============================================================================
 * ESP32 TMC2209 Stepper Driver Example (UART + STEP/DIR)
 * ============================================================================
 * 
 * Hardware Connection (ESP32 NodeMCU-32S <-> TMC2209 Module):
 * 
 *   ESP32 Pin     TMC2209 Pin     Description
 *   -------------------------------------------------------------------------
 *   GPIO 25  -->  STEP            Step pulse input
 *   GPIO 26  -->  DIR             Direction control
 *   GPIO 27  -->  EN              Enable (Active LOW: LOW=Enabled, HIGH=Disabled)
 *   GPIO 16  -->  PDN_UART        ESP32 RX2 (Direct connection to TMC2209 PDN)
 *   GPIO 17  -->  PDN_UART        ESP32 TX2 (Connect through a 1k Ohm resistor!)
 *   3V3      -->  VIO             Logic voltage (3.3V)
 *   GND      -->  GND             Logic ground (Common ground with ESP32)
 *   EXT 12V  -->  VMOT            Motor supply (12V - 24V DC, NEVER disconnect while live!)
 *   EXT GND  -->  GND             Power supply ground (tied to ESP32 GND)
 *   MOTOR    -->  2B, 2A, 1A, 1B  Bipolar stepper motor phases
 *   MS1/MS2  -->  GND             Driver UART address 0b00 (default on most boards)
 * 
 * ============================================================================
 */

// ======================= PIN CONFIGURATION =======================
#define EN_PIN            27  // Driver Enable pin (Active LOW)
#define DIR_PIN           26  // Direction pin
#define STEP_PIN          25  // Step pin
#define DIAG_PIN          14  // Optional StallGuard DIAG pin

#define DRIVER_RX_PIN     16  // ESP32 RX2 connected directly to PDN_UART
#define DRIVER_TX_PIN     17  // ESP32 TX2 connected through 1k resistor to PDN_UART
#define SERIAL_PORT_2     Serial2

// ======================= DRIVER CONFIGURATION ====================
#define DRIVER_ADDRESS    0b00  // TMC2209 driver address (MS1=GND, MS2=GND -> 0)
#define R_SENSE           0.11f // SilentStepStick & BTT TMC2209 typically use 0.11 Ohm

// Create TMC2209 driver instance using HardwareSerial
TMC2209Stepper driver(&SERIAL_PORT_2, R_SENSE, DRIVER_ADDRESS);

// ======================= MOTION CONTROL STATE ====================
enum MotorMode {
    MODE_STOPPED,
    MODE_CONTINUOUS_FWD,
    MODE_CONTINUOUS_REV,
    MODE_TARGET_STEPS,
    MODE_AUTO_DEMO
};

MotorMode currentMode = MODE_STOPPED;
bool driverEnabled = true;

// Stepper specifications
const uint16_t FULL_STEPS_PER_REV = 200; // Standard 1.8 degree stepper motor

// Motion parameters
uint16_t currentMicrosteps = 16;
uint16_t currentRmsCurrent = 600;        // mA
uint32_t stepIntervalMicros = 800;       // Interval between step pulses in microseconds
const uint32_t MIN_STEP_INTERVAL = 100;  // Maximum speed limit (~10,000 steps/s)
const uint32_t MAX_STEP_INTERVAL = 5000; // Minimum speed limit

// Step counting & targeting
uint32_t stepCounter = 0;
uint32_t targetSteps = 0;
unsigned long lastStepTime = 0;

// Auto-demo state variables
enum DemoSubState { DEMO_FWD, DEMO_PAUSE_1, DEMO_REV, DEMO_PAUSE_2 };
DemoSubState demoState = DEMO_FWD;
unsigned long demoStateStartTime = 0;
const unsigned long DEMO_PAUSE_MS = 600;

// ======================= FUNCTION DECLARATIONS ===================
void setDriverEnable(bool enable);
void setMotorDirection(bool forward);
void triggerStepPulse();
void handleStepping();
void printDiagnostics();
void printHelp();
void processSerialCommand(char cmd);
void applyMicrosteps(uint16_t ms);
void applyCurrent(uint16_t mA);

// ======================= SETUP ===================================
void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000);

    Serial.println(F("\n=========================================="));
    Serial.println(F("   ESP32 TMC2209 Stepper Driver Demo      "));
    Serial.println(F("=========================================="));

    // Initialize control pins
    pinMode(EN_PIN, OUTPUT);
    pinMode(DIR_PIN, OUTPUT);
    pinMode(STEP_PIN, OUTPUT);
    pinMode(DIAG_PIN, INPUT_PULLUP);

    digitalWrite(STEP_PIN, LOW);
    setMotorDirection(true);
    setDriverEnable(true);

    // Initialize TMC2209 UART on Serial2
    SERIAL_PORT_2.begin(115200, SERIAL_8N1, DRIVER_RX_PIN, DRIVER_TX_PIN);

    Serial.println(F("[INIT] Initializing TMC2209 via UART..."));
    driver.begin();

    // Test communication
    uint8_t connResult = driver.test_connection();
    if (connResult == 0) {
        Serial.println(F("[INIT] SUCCESS: TMC2209 UART communication established!"));
    } else if (connResult == 1) {
        Serial.println(F("[WARN] Driver responded with 0xFFFFFFFF (Check wiring / 1k resistor)."));
    } else {
        Serial.println(F("[WARN] No response from TMC2209 (Check VMOT power, VIO 3.3V, address)."));
    }

    uint8_t version = driver.version();
    Serial.printf("[INIT] TMC2209 Silicon Version: 0x%02X\n", version);

    // Basic driver configuration
    driver.toff(5);                        // Enable driver internal stage (toff > 0)
    driver.blank_time(24);
    driver.rms_current(currentRmsCurrent); // RMS current in mA
    driver.microsteps(currentMicrosteps);  // Microsteps: 16
    driver.pwm_autoscale(true);            // Required for StealthChop voltage PWM
    driver.en_spreadCycle(false);          // False = StealthChop (ultra-silent)

    // Optional StallGuard threshold (0-255)
    driver.SGTHRS(100);

    Serial.printf("[INIT] Current: %u mA | Microsteps: 1/%u | StealthChop: ON\n",
                  currentRmsCurrent, currentMicrosteps);

    printHelp();
}

// ======================= MAIN LOOP ===============================
void loop() {
    // 1. Process serial commands from user
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c != '\r' && c != '\n') {
            processSerialCommand(c);
        }
    }

    // 2. Execute stepper pulse generation
    handleStepping();
}

// ======================= STEPPING LOGIC ==========================
void setDriverEnable(bool enable) {
    driverEnabled = enable;
    digitalWrite(EN_PIN, enable ? LOW : HIGH); // LOW enables TMC2209
}

void setMotorDirection(bool forward) {
    digitalWrite(DIR_PIN, forward ? HIGH : LOW);
}

void triggerStepPulse() {
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(3); // TMC2209 minimum pulse width is 100ns
    digitalWrite(STEP_PIN, LOW);
}

void handleStepping() {
    if (!driverEnabled || currentMode == MODE_STOPPED) {
        return;
    }

    unsigned long nowMicros = micros();

    // Handle Auto Demo Mode
    if (currentMode == MODE_AUTO_DEMO) {
        unsigned long nowMillis = millis();
        uint32_t oneRevolutionSteps = (uint32_t)FULL_STEPS_PER_REV * currentMicrosteps;

        switch (demoState) {
            case DEMO_FWD:
                setMotorDirection(true);
                if (nowMicros - lastStepTime >= stepIntervalMicros) {
                    lastStepTime = nowMicros;
                    triggerStepPulse();
                    stepCounter++;
                    if (stepCounter >= oneRevolutionSteps) {
                        stepCounter = 0;
                        demoState = DEMO_PAUSE_1;
                        demoStateStartTime = nowMillis;
                    }
                }
                break;

            case DEMO_PAUSE_1:
                if (nowMillis - demoStateStartTime >= DEMO_PAUSE_MS) {
                    demoState = DEMO_REV;
                }
                break;

            case DEMO_REV:
                setMotorDirection(false);
                if (nowMicros - lastStepTime >= stepIntervalMicros) {
                    lastStepTime = nowMicros;
                    triggerStepPulse();
                    stepCounter++;
                    if (stepCounter >= oneRevolutionSteps) {
                        stepCounter = 0;
                        demoState = DEMO_PAUSE_2;
                        demoStateStartTime = nowMillis;
                    }
                }
                break;

            case DEMO_PAUSE_2:
                if (nowMillis - demoStateStartTime >= DEMO_PAUSE_MS) {
                    demoState = DEMO_FWD;
                }
                break;
        }
        return;
    }

    // Handle Target Steps Mode (discrete move)
    if (currentMode == MODE_TARGET_STEPS) {
        if (nowMicros - lastStepTime >= stepIntervalMicros) {
            lastStepTime = nowMicros;
            triggerStepPulse();
            stepCounter++;
            if (stepCounter >= targetSteps) {
                currentMode = MODE_STOPPED;
                stepCounter = 0;
                targetSteps = 0;
                Serial.println(F("[MOTOR] Target steps completed."));
            }
        }
        return;
    }

    // Handle Continuous Modes
    if (currentMode == MODE_CONTINUOUS_FWD || currentMode == MODE_CONTINUOUS_REV) {
        if (nowMicros - lastStepTime >= stepIntervalMicros) {
            lastStepTime = nowMicros;
            triggerStepPulse();
            stepCounter++;
        }
    }
}

// ======================= SERIAL INTERACTION ======================
void applyMicrosteps(uint16_t ms) {
    currentMicrosteps = ms;
    driver.microsteps(currentMicrosteps);
    Serial.printf("[CONFIG] Microstepping set to 1/%u\n", currentMicrosteps);
}

void applyCurrent(uint16_t mA) {
    currentRmsCurrent = mA;
    driver.rms_current(currentRmsCurrent);
    Serial.printf("[CONFIG] RMS Current set to %u mA\n", currentRmsCurrent);
}

void processSerialCommand(char cmd) {
    uint32_t oneRev = (uint32_t)FULL_STEPS_PER_REV * currentMicrosteps;

    switch (cmd) {
        case 'f':
        case 'F':
            setMotorDirection(true);
            currentMode = MODE_CONTINUOUS_FWD;
            Serial.println(F("[CMD] Continuous FORWARD running..."));
            break;

        case 'b':
        case 'B':
            setMotorDirection(false);
            currentMode = MODE_CONTINUOUS_REV;
            Serial.println(F("[CMD] Continuous REVERSE running..."));
            break;

        case 's':
        case 'S':
            currentMode = MODE_STOPPED;
            stepCounter = 0;
            targetSteps = 0;
            Serial.println(F("[CMD] Motor STOPPED."));
            break;

        case '1':
            setMotorDirection(true);
            stepCounter = 0;
            targetSteps = oneRev;
            currentMode = MODE_TARGET_STEPS;
            Serial.printf("[CMD] Rotating 1 revolution FORWARD (%u steps)...\n", oneRev);
            break;

        case '2':
            setMotorDirection(false);
            stepCounter = 0;
            targetSteps = oneRev;
            currentMode = MODE_TARGET_STEPS;
            Serial.printf("[CMD] Rotating 1 revolution REVERSE (%u steps)...\n", oneRev);
            break;

        case 'a':
        case 'A':
            if (currentMode == MODE_AUTO_DEMO) {
                currentMode = MODE_STOPPED;
                Serial.println(F("[CMD] Auto-Demo STOPPED."));
            } else {
                currentMode = MODE_AUTO_DEMO;
                demoState = DEMO_FWD;
                stepCounter = 0;
                Serial.println(F("[CMD] Auto-Demo STARTED (alternating CW/CCW 1 rev)."));
            }
            break;

        case 'e':
        case 'E':
            setDriverEnable(!driverEnabled);
            Serial.printf("[CMD] Driver output %s (EN pin %s)\n",
                          driverEnabled ? "ENABLED" : "DISABLED",
                          driverEnabled ? "LOW" : "HIGH");
            break;

        case '+':
        case '=':
            if (stepIntervalMicros > MIN_STEP_INTERVAL + 100) {
                stepIntervalMicros -= 100;
            } else if (stepIntervalMicros > MIN_STEP_INTERVAL) {
                stepIntervalMicros = MIN_STEP_INTERVAL;
            }
            Serial.printf("[SPEED] Faster: Interval = %u us (~%.1f steps/sec)\n",
                          stepIntervalMicros, 1000000.0f / stepIntervalMicros);
            break;

        case '-':
        case '_':
            if (stepIntervalMicros < MAX_STEP_INTERVAL) {
                stepIntervalMicros += 100;
            }
            Serial.printf("[SPEED] Slower: Interval = %u us (~%.1f steps/sec)\n",
                          stepIntervalMicros, 1000000.0f / stepIntervalMicros);
            break;

        case 'm':
        case 'M': {
            static const uint16_t msList[] = {1, 2, 4, 8, 16, 32, 64, 128, 256};
            static uint8_t msIdx = 4; // index 4 = 16
            msIdx = (msIdx + 1) % 9;
            applyMicrosteps(msList[msIdx]);
            break;
        }

        case 'c':
        case 'C': {
            static const uint16_t currList[] = {300, 500, 700, 900, 1100};
            static uint8_t currIdx = 2; // index 2 = 700
            currIdx = (currIdx + 1) % 5;
            applyCurrent(currList[currIdx]);
            break;
        }

        case 't':
        case 'T': {
            bool sc = driver.en_spreadCycle();
            driver.en_spreadCycle(!sc);
            Serial.printf("[MODE] Switched to %s mode\n",
                          !sc ? "SpreadCycle (Torque)" : "StealthChop (Quiet)");
            break;
        }

        case 'd':
        case 'D':
            printDiagnostics();
            break;

        case '?':
        case 'h':
        case 'H':
            printHelp();
            break;

        default:
            Serial.printf("[?] Unknown command '%c'. Send '?' for help.\n", cmd);
            break;
    }
}

// ======================= DIAGNOSTICS & HELP ======================
void printDiagnostics() {
    Serial.println(F("\n-------------------------------------------"));
    Serial.println(F("       TMC2209 DRIVER DIAGNOSTICS          "));
    Serial.println(F("-------------------------------------------"));

    uint8_t conn = driver.test_connection();
    Serial.printf("UART Connection      : %s (code %u)\n",
                  conn == 0 ? "OK" : (conn == 1 ? "HIGH IMPEDANCE (0xFFFFFFFF)" : "NO RESPONSE (0x00)"),
                  conn);

    uint8_t version = driver.version();
    Serial.printf("Silicon Version      : 0x%02X %s\n",
                  version,
                  (version == 0x21 ? "[TMC2209 VALID]" : "[UNKNOWN/DISCONNECTED]"));

    Serial.printf("Driver Output (EN)   : %s\n", driverEnabled ? "ENABLED (EN=LOW)" : "DISABLED (EN=HIGH)");
    Serial.printf("RMS Motor Current    : %u mA\n", driver.rms_current());
    Serial.printf("Microstep Setting    : 1/%u\n", driver.microsteps());
    Serial.printf("Chopper Mode         : %s\n", driver.en_spreadCycle() ? "SpreadCycle (Torque)" : "StealthChop (Quiet)");

    uint32_t drvStatus = driver.DRV_STATUS();
    Serial.printf("DRV_STATUS Register  : 0x%08X\n", drvStatus);
    Serial.printf(" - Overtemp Prewarn  : %s\n", driver.otpw() ? "WARNING (Hot)" : "Normal");
    Serial.printf(" - Overtemp Shutdown : %s\n", driver.ot() ? "FAULT (Thermal Shutdown)" : "Normal");
    Serial.printf(" - Short to GND CoilA: %s\n", driver.s2ga() ? "FAULT" : "OK");
    Serial.printf(" - Short to GND CoilB: %s\n", driver.s2gb() ? "FAULT" : "OK");
    Serial.printf(" - Open Load Coil A  : %s\n", driver.ola() ? "OPEN LOAD" : "OK");
    Serial.printf(" - Open Load Coil B  : %s\n", driver.olb() ? "OPEN LOAD" : "OK");
    Serial.printf(" - Motor Standstill  : %s\n", driver.stst() ? "YES" : "NO");
    Serial.printf(" - Current Scale (CS): %u / 31\n", driver.cs_actual());
    Serial.printf(" - StallGuard Result : %u\n", driver.SG_RESULT());
    Serial.printf(" - DIAG Pin (GPIO%d)  : %s\n", DIAG_PIN, digitalRead(DIAG_PIN) ? "HIGH" : "LOW (Stall / Trigger)");
    Serial.println(F("-------------------------------------------\n"));
}

void printHelp() {
    Serial.println(F("\nAvailable Serial Commands:"));
    Serial.println(F("  f / b  : Run continuous FORWARD / REVERSE"));
    Serial.println(F("  1 / 2  : Move 1 full revolution FORWARD / REVERSE"));
    Serial.println(F("  a      : Toggle Auto-Demo (alternating CW/CCW 1 revolution)"));
    Serial.println(F("  s      : STOP motor"));
    Serial.println(F("  e      : Toggle driver Enable (EN pin LOW/HIGH)"));
    Serial.println(F("  + / -  : Increase / decrease step speed"));
    Serial.println(F("  m      : Cycle microstepping (1, 2, 4, 8, 16, 32, 64, 128, 256)"));
    Serial.println(F("  c      : Cycle RMS current (300, 500, 700, 900, 1100 mA)"));
    Serial.println(F("  t      : Toggle StealthChop / SpreadCycle chopper modes"));
    Serial.println(F("  d      : Read and display full driver diagnostics via UART"));
    Serial.println(F("  ? / h  : Print this help menu\n"));
}