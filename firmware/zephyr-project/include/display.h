/*
 * Universal Bike Controller
 * Copyright (C) 2022-2023, Greco Engineering Solutions LLC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <string.h>
#include <zephyr/types.h>
#include <zephyr/kernel.h>

#include "common.h"


typedef struct
{
    uint16_t ms;
    uint8_t secs;
    uint8_t mins;
    uint8_t hrs;
    uint32_t last_ms;
    bool running;
} stopwatch_data_t;

int initDisplay();
int updateDisplay ( bike_data_t bikeData );
void resetTime();

#endif  // DISPLAY_H