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

#ifndef CPS_H
#define CPS_H

#include <zephyr/types.h>

#include "common.h"

// Service UUID's
#define BT_UUID_CPS_VAL 0x1818
#define BT_UUID_CPS BT_UUID_DECLARE_16 ( BT_UUID_CPS_VAL )

// Characteristic UUID's
#define BLE_UUID_CYCLING_POWER_MEASUREMENT_CHAR BT_UUID_DECLARE_16 ( 0x2A63 )
#define BLE_UUID_CYCLING_POWER_FEATURE_CHAR BT_UUID_DECLARE_16 ( 0x2A65 )

// 3.57 Cycling Power Feature (GATT Specification Supplement)
// 3.1 Cycling Power Feature (Cycling Power Service Specification)
typedef struct
{
    uint32_t feat_blsc;
} ble_cps_features_t;

// 3.58 Cycling Power Measurement (GATT Specification Supplement
// 3.2 Cycling Power Measurement (Cycling Power Service Specification)
typedef struct
{
    uint16_t flags;
    int16_t InstantaneousPower;  // Watts
} ble_cps_measurement_data_t;

int bt_cps_notify ( bike_data_t bikeData );

#endif  // CPS_H