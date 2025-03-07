/****************************************************************************
 * RATGDO HomeKit for ESP32
 * https://ratcloud.llc
 * https://github.com/PaulWieland/ratgdo
 *
 * Copyright (c) 2023-24 David A Kerr... https://github.com/dkerr64/
 * All Rights Reserved.
 * Licensed under terms of the GPL-3.0 License.
 *
 * Contributions acknowledged from
 * Brandon Matthews... https://github.com/thenewwazoo
 * Jonathan Stroud...  https://github.com/jgstroud
 *
 */
#pragma once

// C/C++ language includes
// none

// ESP system includes
// none

// RATGDO project includes
#define PROGMEM // so it is no-op in webcontent.h
#include "www/build/webcontent.h"

extern void setup_web();
extern void web_loop();

extern void handle_notfound();
extern void handle_reboot();

extern void load_page(const char *page);

extern const char response400invalid[];
extern const char type_txt[];

enum BroadcastType : uint8_t
{
    RATGDO_STATUS = 1,
    LOG_MESSAGE = 2,
};
void SSEBroadcastState(const char *data, BroadcastType type = RATGDO_STATUS);

extern "C" int crashCount; // pull in number of times crashed.
