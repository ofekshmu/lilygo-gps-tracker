# LilyGo SIM7600E GPS Tracker

Firmware for the **LilyGo T-SIM7600E** board that reads GPS coordinates and uploads them to **ThingSpeak** for live map tracking from any phone browser.

## What it does

1. Boots and connects to the internet — either via **4G cellular (SIM)** or **WiFi**, as configured
2. Acquires a GPS fix from the SIM7600E's built-in GPS receiver
3. Sends latitude, longitude, speed, and altitude to a ThingSpeak channel over HTTP
4. Repeats on a configurable interval (default: 60 seconds)

If no GPS fix is obtained within the timeout, a test point (Tel Aviv) is sent so the ThingSpeak pipeline can be verified indoors.

## Hardware

| Component | Details |
|---|---|
| Board | LilyGo T-SIM7600E (ESP32-WROVER-E + SIM7600E-L1C) |
| GPS antenna | Ceramic patch antenna connected to GPS u.FL port |
| LTE antenna | Full Band LTE flex antenna connected to cellular u.FL port |
| SIM card | Partner Israel prepaid (Pelephone network, APN: `internet`) |
| Programming port | TTL (upper USB-C port) |

## Configuration

All user settings live in `data/config.json` (not committed — copy from `data/config.json.template`):

```json
{
  "connection_mode": "cellular",   // "cellular" or "wifi"
  "wifi": {
    "ssid": "YOUR_WIFI_SSID",
    "password": "YOUR_WIFI_PASSWORD"
  },
  "cellular": {
    "apn": "internet"
  },
  "thingspeak": {
    "api_key": "YOUR_THINGSPEAK_WRITE_API_KEY",
    "host": "api.thingspeak.com",
    "port": 80
  },
  "gps_timeout_seconds": 30,
  "update_interval_seconds": 60
}
```

## ThingSpeak channel setup

Create a free channel at [thingspeak.com](https://thingspeak.com) with these fields:

| Field | Value |
|---|---|
| Field 1 | Latitude |
| Field 2 | Longitude |
| Field 3 | Speed (km/h) |
| Field 4 | Altitude (m) |

Add a **Map widget** to the channel dashboard to see live location on your phone.

## Flashing

Requires Python + PlatformIO CLI (`pip install platformio`).

**Double-click `flash.bat`** — this uploads both firmware and config in one step.

Or manually:
```bash
# Flash firmware
python -m platformio run --target upload --upload-port COM3

# Flash config (after any config.json change)
python -m platformio run --target uploadfs --upload-port COM3
```

Connect via the **TTL port** (upper USB-C). The **USB port** (lower) connects to the SIM7600E modem only.

## Serial monitor

```bash
python -m platformio device monitor --port COM3 --baud 115200
```

Expected boot output:
```
=== LilyGo SIM7600E GPS Tracker ===
[CFG] Loaded config.json
[CFG] Connection mode : cellular
[MODE] >>> Using Cellular (SIM) <<<
[SIM] Operator: Pelephone Pelephone
[SIM] IP: 10.x.x.x
[GPS] Waiting for fix......
[GPS] 32.xxxxxx, 34.xxxxxx  speed=0.0 km/h  alt=xx.x m
[TS] Sending update... OK (entry #42)
```
