#include <Arduino.h>
#include "TMC2209Controller.h"
#include "INA226Monitor.h"
#include "WiFiService.h"
#include "StepperWebUI.h"

// Controller and service instances
TMC2209Controller stepper;
INA226Monitor powerMonitor;
WiFiService wifiService;
StepperWebUI webUI(stepper, powerMonitor, wifiService);

// Telemetry stream state
bool liveTelemetryActive = false;
unsigned long lastTelemetryStreamTime = 0;
const unsigned long TELEMETRY_STREAM_INTERVAL_MS = 500;

// Periodic 3-second debug current logging
unsigned long lastDebugPrintTime = 0;
const unsigned long DEBUG_PRINT_INTERVAL_MS = 3000;

// Function declarations
void processSerialCommand(char cmd);
void printHelp();
void printCurrentDebug();
void backgroundServicesTask(void* pvParameters);

// Task handle for Core 0 Background Services
TaskHandle_t bgServicesTaskHandle = NULL;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000);

    Serial.println(F("\n========================================================"));
    Serial.println(F("  ESP32 TMC2209 Stepper Driver & INA226 Power Monitor   "));
    Serial.println(F("========================================================"));

    // 1. Initialize TMC2209 stepper controller
    stepper.begin();

    // 2. Disable Watchdogs on Core 1 (Application CPU) so stepper pulse loop has 100% continuous time
    disableCore1WDT();
    disableLoopWDT();

    // 3. Initialize INA226 voltage and current monitor
    powerMonitor.begin();

    // 4. Initialize WiFi network connection
    wifiService.begin();

    // 5. Initialize WebUI dashboard & REST API
    webUI.begin();

    // 6. Spawn Background Services Task pinned to Core 0 (PRO_CPU)
    // Core 0 handles WiFi, WebServer, INA226 I2C, Serial CLI, and telemetry
    // Core 1 (APP_CPU) is left 100% dedicated to jitter-free stepper pulse generation
    xTaskCreatePinnedToCore(
        backgroundServicesTask,
        "BgServicesCore0",
        8192,
        NULL,
        1,                     // Priority 1 (normal application priority)
        &bgServicesTaskHandle,
        0                      // Pin to Core 0 (PRO_CPU)
    );

    Serial.println(F("\n--------------------------------------------------------"));
    Serial.println(F("  [ARCHITECTURE] DUAL-CORE MULTIPROCESSING ACTIVATED:    "));
    Serial.println(F("   - Core 0 (PRO_CPU): WiFi, WebServer, INA226, Serial CLI"));
    Serial.println(F("   - Core 1 (APP_CPU): 100% Dedicated Stepper Pulse Loop   "));
    Serial.println(F("--------------------------------------------------------\n"));

    printHelp();
}

/**
 * @brief Core 0 Background Worker.
 * Handles all network requests, WebUI polling, I2C telemetry, and Serial I/O.
 * Yields 1ms per iteration so the underlying ESP32 WiFi stack and IDLE0 run without latency.
 */
void backgroundServicesTask(void* pvParameters) {
    while (true) {
        // 1. Process serial commands from user
        while (Serial.available()) {
            char c = (char)Serial.read();
            if (c != '\r' && c != '\n') {
                processSerialCommand(c);
            }
        }

        // 2. Maintain WiFi network connection state
        wifiService.update();

        // 3. Serve WebUI requests
        webUI.update();

        // 4. Periodic live telemetry stream (if enabled)
        if (liveTelemetryActive && (millis() - lastTelemetryStreamTime >= TELEMETRY_STREAM_INTERVAL_MS)) {
            lastTelemetryStreamTime = millis();
            INA226Monitor::Telemetry t = powerMonitor.read();
            if (t.valid) {
                Serial.printf("[PWR] VBUS: %5.2f V | ISHUNT: %6.1f mA | POWER: %5.2f W | %s\n",
                              t.busVoltage_V, t.current_mA, t.power_W,
                              stepper.isEnabled() ? "ENABLED" : "DISABLED");
            }
        }

        // 5. Periodic 3-second debug print (Coil target vs INA226 DC bus current)
        if (millis() - lastDebugPrintTime >= DEBUG_PRINT_INTERVAL_MS) {
            lastDebugPrintTime = millis();
            printCurrentDebug();
        }

        // Yield to FreeRTOS on Core 0 so WiFi stack and IDLE0 run smoothly
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

/**
 * @brief Arduino loop() runs on Core 1 (APP_CPU).
 * Exclusively dedicated to stepper motion control.
 * Zero interference from WiFi packets, WebUI polling, I2C reads, or Serial I/O.
 */
void loop() {
    stepper.update();
}

void processSerialCommand(char cmd) {
    switch (cmd) {
        case 'f':
        case 'F':
            stepper.moveForward();
            break;

        case 'b':
        case 'B':
            stepper.moveReverse();
            break;

        case 's':
        case 'S':
            stepper.stop();
            break;

        case '1':
            stepper.moveRevolutions(1.0f, true);
            break;

        case '2':
            stepper.moveRevolutions(1.0f, false);
            break;

        case 'a':
        case 'A':
            stepper.toggleAutoDemo();
            break;

        case 'e':
        case 'E':
            stepper.toggleEnable();
            break;

        case 'p':
        case 'P':
            powerMonitor.printMetrics(Serial);
            break;

        case 'l':
        case 'L':
            liveTelemetryActive = !liveTelemetryActive;
            Serial.printf("[CMD] Live Telemetry Stream %s.\n",
                          liveTelemetryActive ? "STARTED (500ms interval)" : "STOPPED");
            break;

        case 'w':
        case 'W':
            wifiService.printStatus(Serial);
            break;

        case '+':
        case '=':
            stepper.increaseSpeed(100);
            break;

        case '-':
        case '_':
            stepper.decreaseSpeed(100);
            break;

        case 'm':
        case 'M':
            stepper.cycleMicrosteps();
            break;

        case 'c':
        case 'C':
            stepper.cycleRmsCurrent();
            break;

        case 't':
        case 'T':
            stepper.toggleSpreadCycle();
            break;

        case 'd':
        case 'D':
            stepper.printDiagnostics(Serial);
            powerMonitor.printMetrics(Serial);
            wifiService.printStatus(Serial);
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

void printHelp() {
    Serial.println(F("\nAvailable Serial Commands:"));
    Serial.println(F("  f / b  : Run continuous FORWARD / REVERSE"));
    Serial.println(F("  1 / 2  : Move 1 full revolution FORWARD / REVERSE"));
    Serial.println(F("  a      : Toggle Auto-Demo (alternating CW/CCW 1 revolution)"));
    Serial.println(F("  s      : STOP motor"));
    Serial.println(F("  e      : Toggle driver Enable (EN pin LOW/HIGH)"));
    Serial.println(F("  p      : Read instant INA226 Power & Voltage Telemetry"));
    Serial.println(F("  l      : Toggle Live Telemetry Stream (continuous 500ms)"));
    Serial.println(F("  w      : Read and display WiFi connection status & IP"));
    Serial.println(F("  + / -  : Increase / decrease step speed"));
    Serial.println(F("  m      : Cycle microstepping (1, 2, 4, 8, 16, 32, 64, 128, 256)"));
    Serial.println(F("  c      : Cycle RMS current (300, 500, 700, 900, 1100 mA)"));
    Serial.println(F("  t      : Toggle StealthChop / SpreadCycle chopper modes"));
    Serial.println(F("  d      : Read and display full diagnostics (TMC2209 + INA226 + WiFi)"));
    Serial.println(F("  ? / h  : Print this help menu\n"));
}

void printCurrentDebug() {
    INA226Monitor::Telemetry p = powerMonitor.read();
    TMC2209Controller::DiagTelemetry diag = stepper.getCachedDiag();
    uint16_t targetCoilRms = stepper.getRmsCurrent();

    Serial.printf("[DEBUG 3s] ~%.0f RPM | SG: %u (%s) | Coil: %u mA (CS: %u/31, %s) "
                  "|| INA226: %5.2f V | %6.1f mA | %5.2f W\n",
                  stepper.getRPM(),
                  diag.sg,
                  diag.stall ? "STALL/LOST STEPS" : "OK",
                  targetCoilRms,
                  diag.cs,
                  diag.stst ? "Standstill" : "Running",
                  p.busVoltage_V,
                  p.current_mA,
                  p.power_W);
}