// TINY_GSM_MODEM_SIM7600 and TINY_GSM_RX_BUFFER are set in platformio.ini build_flags
#include <Arduino.h>
#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>
#include "config.h"

HardwareSerial modemSerial(1);
TinyGsm        modem(modemSerial);
TinyGsmClient  gsmClient(modem);
HttpClient     http(gsmClient, THINGSPEAK_HOST, THINGSPEAK_PORT);

// ─── Modem power ─────────────────────────────────────────────────────────────

void powerOnModem() {
    pinMode(MODEM_PWRKEY, OUTPUT);
    pinMode(MODEM_FLIGHT, OUTPUT);
    digitalWrite(MODEM_FLIGHT, HIGH);

    digitalWrite(MODEM_PWRKEY, LOW);
    delay(100);
    digitalWrite(MODEM_PWRKEY, HIGH);
    delay(2000);
    digitalWrite(MODEM_PWRKEY, LOW);
    delay(5000);
}

// ─── Cellular connection ──────────────────────────────────────────────────────

bool connectToNetwork() {
    Serial.print("[NET] Waiting for network registration...");
    if (!modem.waitForNetwork(60000L)) {
        Serial.println(" FAIL");
        return false;
    }
    Serial.println(" OK");
    Serial.printf("[NET] Operator: %s\n", modem.getOperator().c_str());

    // Set APN manually via raw AT commands (more reliable than gprsConnect)
    Serial.print("[NET] Setting APN...");
    modem.sendAT(GF("+CGDCONT=1,\"IP\",\"" APN "\""));
    if (modem.waitResponse(5000) != 1) {
        Serial.println(" FAIL");
        return false;
    }
    Serial.println(" OK");

    Serial.print("[NET] Activating data connection...");
    modem.sendAT(GF("+CGACT=1,1"));
    if (modem.waitResponse(15000) != 1) {
        Serial.println(" FAIL");
        return false;
    }
    Serial.println(" OK");

    // Mark GPRS as connected inside TinyGSM so the TCP client works
    modem.sendAT(GF("+CGPADDR=1"));
    String ipResp = "";
    modem.waitResponse(3000, ipResp);
    Serial.printf("[NET] IP: %s\n", ipResp.c_str());

    return true;
}

// ─── GPS ─────────────────────────────────────────────────────────────────────

bool getGPSFix(float &lat, float &lon, float &speed, float &alt) {
    modem.enableGPS();

    Serial.print("[GPS] Waiting for fix");
    uint32_t start = millis();

    while (millis() - start < GPS_TIMEOUT_MS) {
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
    String path = String("/update?api_key=") + THINGSPEAK_API_KEY
                + "&field1=" + String(lat,   6)
                + "&field2=" + String(lon,   6)
                + "&field3=" + String(speed, 1)
                + "&field4=" + String(alt,   1);

    Serial.print("[TS] Sending update...");

    int err = http.get(path);
    if (err != 0) {
        Serial.printf(" connect error %d\n", err);
        return false;
    }

    int statusCode = http.responseStatusCode();
    String body    = http.responseBody();

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
    Serial.println("\n=== LilyGo SIM7600E GPS Tracker (SIM mode) ===");

    modemSerial.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
    powerOnModem();

    Serial.print("[MODEM] Initializing...");
    if (!modem.init()) {
        Serial.println(" FAIL — restarting in 10 s");
        delay(10000);
        ESP.restart();
    }
    Serial.println(" OK");
    Serial.printf("[MODEM] %s\n", modem.getModemInfo().c_str());

    if (!connectToNetwork()) {
        Serial.println("[NET] Failed — restarting in 10 s");
        delay(10000);
        ESP.restart();
    }
}

void loop() {
    float lat = 0, lon = 0, speed = 0, alt = 0;

    if (getGPSFix(lat, lon, speed, alt)) {
        sendToThingSpeak(lat, lon, speed, alt);
    } else {
        Serial.println("[LOOP] No fix this cycle, skipping upload.");
    }

    Serial.printf("[LOOP] Next update in %lu s\n", UPDATE_INTERVAL_MS / 1000UL);
    delay(UPDATE_INTERVAL_MS);
}
