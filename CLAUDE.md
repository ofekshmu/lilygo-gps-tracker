# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

Firmware for a **LilyGo T-SIM7600E** ESP32 board. The device acquires a GPS fix using the SIM7600E's built-in GPS receiver, then pushes latitude, longitude, speed, and altitude to **ThingSpeak** over the 4G LTE cellular connection. ThingSpeak's map widget is used to visualise the live location on any phone browser.

## Environment

- **Framework**: PlatformIO + Arduino (VS Code extension)
- **Target board**: `esp32dev` (LilyGo T-SIM7600E)
- **Key libraries**: TinyGSM (modem + GPS AT-command abstraction), ArduinoHttpClient

## Common commands

```bash
# Build
pio run

# Flash to connected board
pio run --target upload

# Open serial monitor (115200 baud)
pio device monitor

# Build + flash + monitor in one step
pio run --target upload && pio device monitor
```

## Configuration before flashing

All user-specific values live in `include/config.h` — **never** hardcode them in `src/main.cpp`:

| Constant | What to fill in |
|---|---|
| `APN` | SIM card APN string from your carrier |
| `THINGSPEAK_API_KEY` | Write API Key from your ThingSpeak channel |
| `UPDATE_INTERVAL_MS` | Upload cadence (ThingSpeak free tier min: 15 000 ms) |

## Architecture

```
include/config.h   — all user config (APN, API key, pins, timing)
src/main.cpp       — firmware: modem init → network → GPS fix → ThingSpeak upload → sleep → repeat
platformio.ini     — board, framework, library deps, build flags
```

**Data flow**: `powerOnModem()` → `connectToNetwork()` (GPRS) → `getGPSFix()` (AT+CGPS) → `sendToThingSpeak()` (HTTP GET) → `delay(UPDATE_INTERVAL_MS)` → repeat in `loop()`.

On any fatal init failure (modem or network) the firmware calls `ESP.restart()` after 10 s so the device self-recovers in the field.

## Hardware pins (T-SIM7600E)

Defined in `config.h`. Do not change unless using a different board revision:

| Signal | GPIO |
|---|---|
| MODEM_TX | 27 |
| MODEM_RX | 26 |
| MODEM_PWRKEY | 4 |
| MODEM_FLIGHT | 25 |
| MODEM_STATUS | 36 |

## ThingSpeak channel field mapping

| ThingSpeak field | Value |
|---|---|
| Field 1 | Latitude (decimal degrees, 6 dp) |
| Field 2 | Longitude (decimal degrees, 6 dp) |
| Field 3 | Speed (km/h) |
| Field 4 | Altitude (m) |
