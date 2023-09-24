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

#ifndef COMMON_H
#define COMMON_H

#include <zephyr/types.h>

typedef struct
{
    uint8_t nodeId;
    uint8_t funcCode;
    uint16_t dataAddress;
    uint16_t value;
} cmd_msg_data_t;

typedef struct
{
    int16_t incline;     // 0.01% - 0x7FFF invalid
    uint8_t resistance;  // 0.5% - 0xFF invalid
} bike_tgts_t;
typedef void ( *set_targets_callback_t ) ( const bike_tgts_t );

typedef struct
{
    uint16_t disp_res;
    uint16_t watts;
    uint16_t act_rpm;
    uint16_t tgt_inc;
} bike_data_t;

#endif  // COMMON_H