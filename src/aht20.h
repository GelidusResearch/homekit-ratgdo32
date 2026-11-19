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
#pragma once

#ifdef USE_AHT20
#if !defined(USE_DHT22) || !defined(RATGDO32_DISCO)
#error "USE_AHT20 needs USE_DHT22 (it shares its HomeKit accessory) and RATGDO32_DISCO (its I2C pins)"
#endif
extern bool aht20_detected;
extern void setup_aht20();
// Starts a measurement, which aht20_read() returns on a pass at least 100ms later
extern void aht20_start();
// True once per aht20_start(), with the reading, or NAN if it failed
extern bool aht20_read(float &celsius, float &humidity);
#else
constexpr bool aht20_detected = false;
#endif
