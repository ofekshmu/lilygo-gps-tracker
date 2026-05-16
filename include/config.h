#pragma once

// ─── WiFi ────────────────────────────────────────────────────────────────────
#define WIFI_SSID  "Ofek&Yuval"
#define WIFI_PASS  "ofek1020"

// ─── Cellular (unused for now — switch back later for field use) ──────────────
#define APN       "internet"   // Partner Israel
#define APN_USER  ""
#define APN_PASS  ""

// ─── ThingSpeak ───────────────────────────────────────────────────────────────
// Create a free channel at https://thingspeak.com with 4 fields:
//   Field 1 = Latitude
//   Field 2 = Longitude
//   Field 3 = Speed (km/h)
//   Field 4 = Altitude (m)
// Then paste your channel's Write API Key below.
#define THINGSPEAK_API_KEY  "YSTYEBP0TI1PKV2L"
#define THINGSPEAK_HOST     "api.thingspeak.com"
#define THINGSPEAK_PORT     80

// ─── LilyGo T-SIM7600E pins ──────────────────────────────────────────────────
#define MODEM_TX        27
#define MODEM_RX        26
#define MODEM_PWRKEY    4
#define MODEM_FLIGHT    25
#define MODEM_STATUS    36

// ─── Timing ───────────────────────────────────────────────────────────────────
// How often to read GPS and push to ThingSpeak (milliseconds).
// ThingSpeak free tier allows one update per 15 seconds minimum.
#define UPDATE_INTERVAL_MS  60000UL   // 1 minute

// Max time to wait for a GPS fix before giving up and retrying next cycle (ms).
#define GPS_TIMEOUT_MS      300000UL  // 5 minutes
