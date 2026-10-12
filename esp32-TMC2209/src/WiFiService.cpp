#include "WiFiService.h"

WiFiService::WiFiService() {
}

WiFiService::~WiFiService() {
}

bool WiFiService::begin() {
    return begin(Config());
}

bool WiFiService::begin(const Config& config) {
    _config = config;

    Serial.println(F("\n[INIT] Initializing WiFi Service..."));
    Serial.printf("[INIT] Connecting to SSID: \"%s\"\n", _config.ssid);

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    // Apply Static IP configuration if requested
    if (_config.useStaticIP) {
        IPAddress localIP, gatewayIP, subnetIP, dnsIP;
        bool validIP = localIP.fromString(_config.staticIP);
        bool validGW = gatewayIP.fromString(_config.gateway);
        bool validSub = subnetIP.fromString(_config.subnet);
        bool validDNS = dnsIP.fromString(_config.dns);

        if (validIP && validGW && validSub && validDNS) {
            if (WiFi.config(localIP, gatewayIP, subnetIP, dnsIP)) {
                Serial.printf("[WIFI] Static IP configured: %s (Gateway: %s, Subnet: %s, DNS: %s)\n",
                              _config.staticIP, _config.gateway, _config.subnet, _config.dns);
            } else {
                Serial.println(F("[WIFI] Warning: Static IP configuration was rejected by network stack."));
            }
        } else {
            Serial.println(F("[WIFI] Error: Could not parse one or more static IP strings."));
        }
    } else {
        Serial.println(F("[WIFI] Using DHCP dynamic IP assignment."));
    }

    WiFi.begin(_config.ssid, _config.password);

    // Brief initial connection wait
    unsigned long start = millis();
    Serial.print(F("[WIFI] Connecting"));
    while (WiFi.status() != WL_CONNECTED && (millis() - start < _config.connectionTimeoutMs)) {
        delay(500);
        Serial.print('.');
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        _wasConnected = true;
        Serial.println(F("[INIT] SUCCESS: WiFi connected!"));
        Serial.printf("[INIT] IP Address: %s | RSSI: %d dBm | MAC: %s\n",
                      WiFi.localIP().toString().c_str(), WiFi.RSSI(), WiFi.macAddress().c_str());
        return true;
    } else {
        _wasConnected = false;
        Serial.println(F("[WARN] WiFi not yet connected. Will continue retrying in background."));
        _lastReconnectAttempt = millis();
        return false;
    }
}

void WiFiService::update() {
    bool currentlyConnected = (WiFi.status() == WL_CONNECTED);

    if (currentlyConnected && !_wasConnected) {
        _wasConnected = true;
        Serial.println(F("\n[WIFI] Connected to network!"));
        Serial.printf("[WIFI] IP Address: %s | RSSI: %d dBm\n",
                      WiFi.localIP().toString().c_str(), WiFi.RSSI());
    } else if (!currentlyConnected && _wasConnected) {
        _wasConnected = false;
        Serial.println(F("\n[WIFI] Connection lost! Retrying in background..."));
        _lastReconnectAttempt = millis();
    } else if (!currentlyConnected) {
        if (millis() - _lastReconnectAttempt >= _config.reconnectIntervalMs) {
            _lastReconnectAttempt = millis();
            WiFi.reconnect();
        }
    }
}

bool WiFiService::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

IPAddress WiFiService::getIP() const {
    return WiFi.localIP();
}

int8_t WiFiService::getRSSI() const {
    return WiFi.RSSI();
}

String WiFiService::getSSID() const {
    return String(_config.ssid);
}

String WiFiService::getMacAddress() const {
    return WiFi.macAddress();
}

void WiFiService::printStatus(Print& out) {
    out.println(F("-------------------------------------------"));
    out.println(F("           WIFI SERVICE STATUS             "));
    out.println(F("-------------------------------------------"));
    out.printf("SSID                 : %s\n", _config.ssid);
    out.printf("Connection Status    : %s\n", isConnected() ? "CONNECTED" : "DISCONNECTED / RETRYING");
    if (isConnected()) {
        out.printf("IP Address           : %s %s\n",
                   WiFi.localIP().toString().c_str(),
                   _config.useStaticIP ? "(Static IP)" : "(DHCP)");
        out.printf("Subnet Mask          : %s\n", WiFi.subnetMask().toString().c_str());
        out.printf("Gateway              : %s\n", WiFi.gatewayIP().toString().c_str());
        out.printf("DNS Server           : %s\n", WiFi.dnsIP().toString().c_str());
        out.printf("Signal Strength(RSSI): %d dBm\n", WiFi.RSSI());
        out.printf("MAC Address          : %s\n", WiFi.macAddress().c_str());
    } else {
        out.printf("Configured Target IP : %s %s\n",
                   _config.staticIP, _config.useStaticIP ? "(Static)" : "(DHCP)");
    }
    out.println(F("-------------------------------------------\n"));
}
