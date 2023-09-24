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

#ifndef FTMS_H
#define FTMS_H

#include <zephyr/types.h>

#include "common.h"

// OpCodes
#define OPCODE_REQUEST 0x00
#define OPCODE_RESET 0x01
#define OPCODE_SET_INC 0x03
#define OPCODE_SET_RES 0x04
#define OPCODE_START 0x07
#define OPCODE_SIM_PARAMS 0x11

#define OPCODE_RESPONSE 0x80
#define OPCODE_SUCCESS 0x01

#define OPCODE_STARTED 0x04

// Service UUID's
#define BT_UUID_FTMS_VAL 0x1826
#define BT_UUID_FTMS BT_UUID_DECLARE_16 ( BT_UUID_FTMS_VAL )

// Characteristic UUID's
#define BLE_UUID_FTMS_FEATURE_CHAR BT_UUID_DECLARE_16 ( 0x2ACC )
#define BLE_UUID_INDOOR_BIKE_DATA_CHAR BT_UUID_DECLARE_16 ( 0x2AD2 )
#define BLUE_UUID_SUPPORTED_INCLINATION_RANGE_CHAR BT_UUID_DECLARE_16 ( 0x2AD5 )
#define BLUE_UUID_SUPPORTED_RESISTANCE_RANGE_CHAR BT_UUID_DECLARE_16 ( 0x2AD6 )
#define BLUE_UUID_FITNESS_CONTROL_POINT_CHAR BT_UUID_DECLARE_16 ( 0x2AD9 )
#define BLE_UUID_FTMS_STATUS_CHAR BT_UUID_DECLARE_16 ( 0x2ADA )

// 4.3.1.1 Fitness Machine Features Field
#define BLE_FTMS_FEATURE_CADENCE_SUPPORTED_BIT BIT ( 1 )
#define BLE_FTMS_FEATURE_POWER_MEASUREMENT_SUPPORTED_BIT BIT ( 14 )

// 4.3.1.2 Target Setting Features Field
#define BLE_FTMS_TARGET_INCLINATION_SUPPORTED_BIT BIT ( 1 )
#define BLE_FTMS_TARGET_RESISTANCE_SUPPORTED_BIT BIT ( 2 )
#define BLE_FTMS_BIKE_SIMULATION_SUPPORTED_BIT BIT ( 13 )

// 3.116 Indoor Bike Data (GATT Specification Supplement)
// 4.9.1 Characteristic Behavior (Fitness Machine Service Specification)
#define BLE_FTMS_INDOOR_FLAGS_FIELD_INSTANTANEOUS_CADENCE_PRESENT BIT ( 2 )
#define BLE_FTMS_INDOOR_FLAGS_FIELD_INSTANTANEOUS_POWER_PRESENT BIT ( 6 )

//  Read feature callback
// 4.3 Fitness Machine Feature
typedef struct __attribute__ ( ( __packed__ ) )
{
    uint32_t feat_blsc;  // 4.3.1.1 Fitness Machine Features Field
    uint32_t tgt_blsc;   // 4.3.1.2 Target Setting Features Field
} ble_ftms_features_t;

//  Read resistance range callback
typedef struct __attribute__ ( ( __packed__ ) )
{
    uint8_t min_cnt;
    uint8_t max_cnt;
    uint8_t inc_cnt;
} ble_ftms_resistance_range_data_t;

//  Read inclination range callback
typedef struct __attribute__ ( ( __packed__ ) )
{
    int16_t min_tenth_pct;
    int16_t max_tenth_pct;
    uint16_t inc_tenth_pct;
} ble_ftms_inclination_range_data_t;

// 4.16 Fitness Machine Control Point
typedef struct __attribute__ ( ( __packed__ ) )
{
    uint8_t req_op;
    uint8_t param [];
} ctrl_point_req_t;

// 4.16.2.18 Set Indoor Bike Simulation Parameters Procedure
typedef struct __attribute__ ( ( __packed__ ) )
{
    int16_t wind_mps;
    int16_t grade_hundredths_pct;
    uint8_t Crr;  // 0.0001
    uint8_t Cw;   // 0.01
} sim_data_param_t;

// 4.16.2.22 Procedure Complete
typedef struct __attribute__ ( ( __packed__ ) )
{
    uint8_t resp_op;
    uint8_t req_op;
    uint8_t result;
    uint8_t param [];
} ctrl_point_resp_t;

// Indoor bike data
typedef struct __attribute__ ( ( __packed__ ) )
{
    uint16_t flags;
    uint16_t InstantaneousSpeed;    // 1/100 of kph
    uint16_t InstantaneousCadence;  // 1/2 of rpm
    int16_t InstantaneousPower;     // watts
} ble_ftms_indoor_bike_data_t;

// 4.17 Fitness Machine Status
typedef struct ftms_status
{
    uint8_t op;
    uint8_t param [];
} ftms_status_t;

// Functions
void ftmsSetTargetsCb ( set_targets_callback_t func ); 
int bt_ftms_bike_notify ( bike_data_t bikeData );
int bt_ftms_status_notify();

#endif  // FTMS_H