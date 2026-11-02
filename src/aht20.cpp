/****************************************************************************
 * RATGDO HomeKit
 * https://ratcloud.llc
 * https://github.com/PaulWieland/ratgdo
 *
 * Copyright (c) 2023-25 David A Kerr... https://github.com/dkerr64/
 * All Rights Reserved.
 * Licensed under terms of the GPL-3.0 License.
 *
 */

// RATGDO project includes
#include "aht20.h"

bool aht20_detected = false;

#ifdef USE_AHT20

// Arduino includes
#include <Wire.h>
#include <Adafruit_AHTX0.h>

// RATGDO project includes
#include "ratgdo.h"
#include "config.h"
#include "vehicle.h"

// Logger tag
static const char *TAG = "ratgdo-aht20";

static const uint8_t AHT20_I2C_ADDRESS = 0x38;

static Adafruit_AHTX0 aht;

// The VL53L1X and AHT20 share the single Wire controller on different pins,
// so the bus is switched to the AHT20 pins for each access and back again.
static bool select_aht20_bus()
{
    Wire.end();
    return Wire.begin(AHT20_SDA_PIN, AHT20_SCL_PIN);
}

static void restore_sensor_bus()
{
    Wire.end();
    if (vehicle_setup_done)
        Wire.begin(SENSOR_SDA_PIN, SENSOR_SCL_PIN);
}

void setup_aht20()
{
    ESP_LOGI(TAG, "=== Probing for AHT20 temperature/humidity sensor ===");

    delay(250);

    bool found = select_aht20_bus() && aht.begin(&Wire);
    restore_sensor_bus();

    if (!found)
    {
        ESP_LOGE(TAG, "AHT20 I2C initialization failed");
        return;
    }

    ESP_LOGI(TAG, "AHT20 detected and initialized");
    aht20_detected = true;
}

bool read_aht20(float &tempC, float &humidity)
{
    if (!aht20_detected)
        return false;

    if (!select_aht20_bus())
    {
        restore_sensor_bus();
        return false;
    }
    sensors_event_t humidityEvent, tempEvent;
    bool ok = aht.getEvent(&humidityEvent, &tempEvent);
    restore_sensor_bus();
    if (!ok)
        return false;

    tempC = tempEvent.temperature;
    humidity = humidityEvent.relative_humidity;
    return !isnan(tempC) && !isnan(humidity);
}

#else // !USE_AHT20

void setup_aht20() {}

bool read_aht20(float &tempC, float &humidity)
{
    (void)tempC;
    (void)humidity;
    return false;
}

#endif // USE_AHT20
