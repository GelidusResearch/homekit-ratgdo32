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

// Logger tag
static const char *TAG = "ratgdo-aht20";

static const uint8_t AHT20_I2C_ADDRESS = 0x38;

static Adafruit_AHTX0 aht;

void setup_aht20()
{
    ESP_LOGI(TAG, "=== Probing for AHT20 temperature/humidity sensor ===");

    if (Wire.begin(AHT20_SDA_PIN, AHT20_SCL_PIN))
    {
        ESP_LOGI(TAG, "I2C pins configured for AHT20 sensor");
    }

    delay(250);

    if (!aht.begin(&Wire))
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

    sensors_event_t humidityEvent, tempEvent;
    if (!aht.getEvent(&humidityEvent, &tempEvent))
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
