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

#ifndef CSCS_H
#define CSCS_H

#include <zephyr/types.h>

#include "common.h"

// Characteristic UUID's
#define BLE_UUID_CSCS_FEATURE_CHAR BT_UUID_DECLARE_16 ( 0x2A5C )
#define BLE_UUID_CSCS_MEASUREMENT_CHAR BT_UUID_DECLARE_16 ( 0x2A5B )

// 3.2
#define BLE_CSCS_FEATURE_CRANK_REV BIT ( 1 )

typedef struct __attribute__ ( ( __packed__ ) )
{
    uint16_t feat_blsc;
} ble_cscs_features_t;

// 3.1
#define BLE_CSCS_CRANK_FLAGS_FIELD BIT ( 1 )

typedef struct __attribute__ ( ( __packed__ ) )
{
    uint8_t flags;
    uint16_t totalRevs_cnt;
    uint16_t lastCrank_1024;
} ble_cscs_measurement_data_t;

int bt_cscs_bike_notify ( bike_data_t bikeData );

#endif  // CSCS_H