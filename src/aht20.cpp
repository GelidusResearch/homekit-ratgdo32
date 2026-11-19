/****************************************************************************
 * RATGDO HomeKit for ESP32
 * https://ratcloud.llc
 * https://github.com/PaulWieland/ratgdo
 *
 * Copyright (c) 2023-26 David A Kerr... https://github.com/dkerr64/
 * All Rights Reserved.
 * Licensed under terms of the GPL-3.0 License.
 *
 * Contributions acknowledged from
 * GelidusResearch... https://github.com/GelidusResearch
 *
 */
#ifdef USE_AHT20
// Arduino includes
#include <Wire.h>

// RATGDO project includes
#include "ratgdo.h"
#include "vehicle.h"
#include "provision.h"
#include "aht20.h"

// Logger tag
static const char *TAG = "ratgdo-aht20";

// A board can wire the AHT20 to other pins than the distance sensor
#ifndef AHT20_SDA_GPIO
#define AHT20_SDA_GPIO SENSOR_SDA_PIN
#endif
#ifndef AHT20_SCL_GPIO
#define AHT20_SCL_GPIO SENSOR_SCL_PIN
#endif

// From the Aosong AHT20 datasheet, sections 5.3 and 5.4
static const uint8_t AHT20_ADDRESS = 0x38;
static const uint8_t AHT20_INIT[] = {0xBE, 0x08, 0x00};
static const uint8_t AHT20_TRIGGER[] = {0xAC, 0x33, 0x00};
static const uint8_t AHT20_BUSY = 0x80;
static const uint8_t AHT20_CALIBRATED = 0x08;
static const unsigned long AHT20_MEASURE_MS = 100; // datasheet: wait 80ms

bool aht20_detected = false;
static bool measuring = false;
static bool triggered = false;
static unsigned long measureStart = 0;

// Wire's default 50ms timeout bounds every transfer
static bool aht20_command(const uint8_t *cmd)
{
    Wire.beginTransmission(AHT20_ADDRESS);
    Wire.write(cmd, 3);
    return Wire.endTransmission() == 0;
}

// Datasheet section 6: 20 bits of humidity, then 20 bits of temperature
static void aht20_decode(const uint8_t *data, float &celsius, float &humidity)
{
    uint32_t rawHumidity = ((uint32_t)data[1] << 12) | (data[2] << 4) | (data[3] >> 4);
    uint32_t rawTemperature = ((uint32_t)(data[3] & 0x0F) << 16) | (data[4] << 8) | data[5];
    humidity = rawHumidity * 100.0f / (1 << 20);
    celsius = rawTemperature * 200.0f / (1 << 20) - 50;
}

static bool aht20_probe()
{
    Wire.beginTransmission(AHT20_ADDRESS);
    if (Wire.endTransmission() != 0 || Wire.requestFrom(AHT20_ADDRESS, (size_t)1) != 1)
        return false;
    // Initialize it if the status byte isn't calibrated, then wait 10ms
    if (!(Wire.read() & AHT20_CALIBRATED) && aht20_command(AHT20_INIT))
        delay(10);
    return true;
}

void setup_aht20()
{
    if (vehicle_setup_done)
    {
        // Wire is already up on the distance sensor's pins
        aht20_detected = aht20_probe();
    }
    else
    {
#ifdef GRGDO1_V1
        // GPIO1/3 are shared with UART0, as for the distance sensor
        release_serial_console_for_i2c();
#endif
        aht20_detected = Wire.begin(AHT20_SDA_GPIO, AHT20_SCL_GPIO) && aht20_probe();
        if (!aht20_detected)
        {
            Wire.end();
#ifdef GRGDO1_V1
            restore_serial_console_after_i2c();
#endif
        }
    }
    ESP_LOGI(TAG, "AHT20 temperature/humidity sensor %s", aht20_detected ? "found" : "not found");
}

void aht20_start()
{
    triggered = aht20_command(AHT20_TRIGGER);
    measureStart = millis();
    measuring = true;
}

bool aht20_read(float &celsius, float &humidity)
{
    if (!measuring || millis() - measureStart < AHT20_MEASURE_MS)
        return false;
    measuring = false;

    uint8_t data[6];
    if (triggered && Wire.requestFrom(AHT20_ADDRESS, sizeof(data)) == sizeof(data))
    {
        for (uint8_t &b : data)
            b = Wire.read();
        if (!(data[0] & AHT20_BUSY))
        {
            aht20_decode(data, celsius, humidity);
            return true;
        }
    }
    ESP_LOGW(TAG, "AHT20 read failed");
    celsius = humidity = NAN;
    return true;
}
#endif // USE_AHT20
