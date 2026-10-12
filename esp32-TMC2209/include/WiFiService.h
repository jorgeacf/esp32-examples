#pragma once

#include <Arduino.h>
#include <WiFi.h>

// ======================= WIFI CONFIGURATION =======================
#define CONF_WIFI_SSID         "WPDHAKSKOWDS"
#define CONF_WIFI_PASSWORD     "UNHQsbwdeKdU"
#define CONF_USE_STATIC_IP     true
#define CONF_STATIC_IP         "192.168.0.203"
#define CONF_GATEWAY           "192.168.0.1"
#define CONF_SUBNET            "255.255.255.0"
#define CONF_DNS               "192.168.0.1"

/**
 * @brief Manages WiFi connection lifecycle, static/DHCP configuration,
 * background auto-reconnection, and network status queries.
 */
class WiFiService {
public:
    struct Config {
        const char* ssid = CONF_WIFI_SSID;
        const char* password = CONF_WIFI_PASSWORD;
        bool useStaticIP = CONF_USE_STATIC_IP;
        const char* staticIP = CONF_STATIC_IP;
        const char* gateway = CONF_GATEWAY;
        const char* subnet = CONF_SUBNET;
        const char* dns = CONF_DNS;
        uint32_t connectionTimeoutMs = 10000;
        uint32_t reconnectIntervalMs = 10000;
    };

    WiFiService();
    ~WiFiService();

    bool begin();
    bool begin(const Config& config);

    /**
     * @brief Periodic handler to maintain connection and auto-reconnect if lost.
     */
    void update();

    bool isConnected() const;
    IPAddress getIP() const;
    int8_t getRSSI() const;
    String getSSID() const;
    String getMacAddress() const;

    void printStatus(Print& out = Serial);

    const Config& getConfig() const { return _config; }

private:
    Config _config;
    bool _wasConnected = false;
    unsigned long _lastReconnectAttempt = 0;
};
