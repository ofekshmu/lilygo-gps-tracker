// TINY_GSM_MODEM_SIM7600 and TINY_GSM_RX_BUFFER are set in platformio.ini build_flags
#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>
#include "config.h"

// ─── Runtime config (loaded from data/config.json) ───────────────────────────
static String  g_mode;               // "cellular" or "wifi"
static String  g_wifiSsid;
static String  g_wifiPass;
static String  g_apn;
static String  g_tsApiKey;
static String  g_tsHost;
static int     g_tsPort;
static bool     g_skipGps;
static uint32_t g_gpsTimeoutMs;
static uint32_t g_updateIntervalMs;

HardwareSerial modemSerial(1);
TinyGsm        modem(modemSerial);

// HTTP client is created after config is loaded (pointer swapped based on mode)
WiFiClient     wifiClient;
TinyGsmClient  gsmClient(modem);
HttpClient*    http = nullptr;

// ─── Config loader ────────────────────────────────────────────────────────────

void loadConfig() {
    if (!LittleFS.begin(true)) {
        Serial.println("[CFG] LittleFS mount failed — using defaults");
        g_mode             = "cellular";
        g_apn              = "internet";
        g_tsApiKey         = "YSTYEBP0TI1PKV2L";
        g_tsHost           = "api.thingspeak.com";
        g_tsPort           = 80;
        g_gpsTimeoutMs     = 30000;
        g_updateIntervalMs = 60000;
        return;
    }

    File f = LittleFS.open("/config.json", "r");
    if (!f) {
        Serial.println("[CFG] config.json not found — using defaults");
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        Serial.printf("[CFG] JSON parse error: %s — using defaults\n", err.c_str());
        return;
    }

    g_mode             = doc["connection_mode"]  | "cellular";
    g_wifiSsid         = doc["wifi"]["ssid"]    | "";
    g_wifiPass         = doc["wifi"]["password"] | "";
    g_apn              = doc["cellular"]["apn"]  | "internet";
    g_tsApiKey         = doc["thingspeak"]["api_key"] | "";
    g_tsHost           = doc["thingspeak"]["host"]    | "api.thingspeak.com";
    g_tsPort           = doc["thingspeak"]["port"]    | 80;
    g_skipGps          = doc["skip_gps"]                            | false;
    g_gpsTimeoutMs     = (uint32_t)(doc["gps_timeout_seconds"]     | 30)  * 1000;
    g_updateIntervalMs = (uint32_t)(doc["update_interval_seconds"] | 60)  * 1000;

    Serial.println("[CFG] Loaded config.json");
    Serial.printf("[CFG] Connection mode : %s\n", g_mode.c_str());
    Serial.printf("[CFG] Skip GPS        : %s\n",    g_skipGps ? "yes" : "no");
    Serial.printf("[CFG] GPS timeout     : %lu s\n", g_gpsTimeoutMs / 1000);
    Serial.printf("[CFG] Update interval : %lu s\n", g_updateIntervalMs / 1000);
    if (g_updateIntervalMs < 15000)
        Serial.println("[CFG] WARNING: update_interval_seconds < 15 — ThingSpeak free tier will reject updates!");
}

// ─── Modem power ─────────────────────────────────────────────────────────────

void powerOnModem() {
    pinMode(MODEM_PWRKEY, OUTPUT);
    pinMode(MODEM_FLIGHT, OUTPUT);
    digitalWrite(MODEM_FLIGHT, HIGH);

    // Check if modem is already on before pulsing PWRKEY.
    // On ESP.restart() the SIM7600E stays powered — a second pulse would turn it OFF.
    modemSerial.println("AT");
    delay(500);
    if (modemSerial.find("OK")) {
        Serial.println("[MODEM] Already on, skipping power pulse");
        return;
    }

    digitalWrite(MODEM_PWRKEY, LOW);
    delay(100);
    digitalWrite(MODEM_PWRKEY, HIGH);
    delay(2000);
    digitalWrite(MODEM_PWRKEY, LOW);
    delay(5000);
}

// ─── WiFi connection ─────────────────────────────────────────────────────────

bool connectToWiFi() {
    Serial.printf("[WiFi] Connecting to \"%s\"...", g_wifiSsid.c_str());
    WiFi.begin(g_wifiSsid.c_str(), g_wifiPass.c_str());

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > 20000) { Serial.println(" FAIL"); return false; }
        delay(500);
        Serial.print(".");
    }
    Serial.println(" OK");
    Serial.printf("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
}

// ─── Cellular connection ──────────────────────────────────────────────────────

bool connectToCellular() {
    Serial.print("[SIM] Waiting for network registration...");
    if (!modem.waitForNetwork(60000L)) { Serial.println(" FAIL"); return false; }
    Serial.println(" OK");
    Serial.printf("[SIM] Operator: %s\n", modem.getOperator().c_str());

    Serial.print("[SIM] Setting APN...");
    modem.sendAT(GF("+CGDCONT=1,\"IP\",\""), g_apn.c_str(), GF("\""));
    if (modem.waitResponse(5000) != 1) { Serial.println(" FAIL"); return false; }
    Serial.println(" OK");

    Serial.print("[SIM] Activating data connection...");
    modem.sendAT(GF("+CGACT=1,1"));
    if (modem.waitResponse(15000) != 1) { Serial.println(" FAIL"); return false; }
    Serial.println(" OK");

    // Open TCP/IP stack — required before TinyGsmClient can make connections.
    // If already open (ERROR response), that's fine — continue.
    Serial.print("[SIM] Opening network service...");
    modem.sendAT(GF("+NETOPEN"));
    int8_t netRes = modem.waitResponse(15000);
    if (netRes == 1) {
        Serial.println(" OK");
    } else {
        // May already be open from a previous session — check
        modem.sendAT(GF("+NETOPEN?"));
        String st; modem.waitResponse(3000, st);
        if (st.indexOf("+NETOPEN: 1") >= 0) {
            Serial.println(" already open");
        } else {
            Serial.println(" FAIL");
            return false;
        }
    }

    modem.sendAT(GF("+CGPADDR=1"));
    String ipResp;
    modem.waitResponse(3000, ipResp);
    ipResp.trim();
    Serial.printf("[SIM] IP: %s\n", ipResp.c_str());
    return true;
}

// ─── GPS ─────────────────────────────────────────────────────────────────────

bool getGPSFix(float &lat, float &lon, float &speed, float &alt) {
    modem.enableGPS();

    Serial.print("[GPS] Waiting for fix");
    uint32_t start = millis();

    while (millis() - start < g_gpsTimeoutMs) {
        float accuracy;
        int vsat, usat, year, month, day, hour, minute, second;

        if (modem.getGPS(&lat, &lon, &speed, &alt,
                         &vsat, &usat, &accuracy,
                         &year, &month, &day, &hour, &minute, &second)) {
            if (lat != 0.0f && lon != 0.0f) {
                Serial.println(" OK");
                Serial.printf("[GPS] %.6f, %.6f  speed=%.1f km/h  alt=%.1f m\n",
                              lat, lon, speed, alt);
                return true;
            }
        }
        Serial.print(".");
        delay(5000);
    }

    Serial.println(" TIMEOUT");
    modem.disableGPS();
    return false;
}

// ─── ThingSpeak ──────────────────────────────────────────────────────────────

bool sendToThingSpeak(float lat, float lon, float speed, float alt) {
    String path = String("/update?api_key=") + g_tsApiKey
                + "&field1=" + String(lat,   6)
                + "&field2=" + String(lon,   6)
                + "&field3=" + String(speed, 1)
                + "&field4=" + String(alt,   1);

    Serial.print("[TS] Sending update...");

    int err = http->get(path);
    if (err != 0) { Serial.printf(" connect error %d\n", err); return false; }

    int statusCode = http->responseStatusCode();
    String body    = http->responseBody();

    if (statusCode == 200 && body.toInt() > 0) {
        Serial.printf(" OK (entry #%s)\n", body.c_str());
        return true;
    }
    Serial.printf(" FAIL (HTTP %d, body: \"%s\")\n", statusCode, body.c_str());
    return false;
}

// ─── Arduino entry points ────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n=== LilyGo SIM7600E GPS Tracker ===");

    loadConfig();

    modemSerial.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
    powerOnModem();

    Serial.print("[MODEM] Initializing...");
    if (!modem.init()) {
        Serial.println(" FAIL — restarting in 10 s");
        delay(10000);
        ESP.restart();
    }
    Serial.println(" OK");

    if (g_mode == "wifi") {
        Serial.println("[MODE] >>> Using WiFi <<<");
        if (!connectToWiFi()) { delay(10000); ESP.restart(); }
        http = new HttpClient(wifiClient, g_tsHost.c_str(), g_tsPort);
    } else {
        Serial.println("[MODE] >>> Using Cellular (SIM) <<<");
        if (!connectToCellular()) { delay(10000); ESP.restart(); }
        http = new HttpClient(gsmClient, g_tsHost.c_str(), g_tsPort);
    }
}

void loop() {
    float lat = 0, lon = 0, speed = 0, alt = 0;
    bool hasFix = false;

    if (!g_skipGps) {
        hasFix = getGPSFix(lat, lon, speed, alt);
    }

    if (hasFix) {
        sendToThingSpeak(lat, lon, speed, alt);
    } else {
        if (!g_skipGps) Serial.println("[LOOP] No GPS fix — sending test point");
        sendToThingSpeak(32.0853, 34.7818, 0.0, 0.0);
    }

    Serial.printf("[LOOP] Next update in %lu s\n", g_updateIntervalMs / 1000);
    delay(g_updateIntervalMs);
}
