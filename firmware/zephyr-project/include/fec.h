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

#ifndef FEC_H
#define FEC_H

#include <zephyr/bluetooth/uuid.h>
#include <zephyr/types.h>

#include "common.h"

// Service UUID's
#define BT_UUID_FEC_VAL \
    BT_UUID_128_ENCODE ( 0x6e40FEC1, 0xb5a3, 0xf393, 0xe0a9, 0xe50e24dcca9e )
#define BT_UUID_FEC BT_UUID_DECLARE_128 ( BT_UUID_FEC_VAL )

// Characteristic UUID's
#define BT_UUID_FEC_RX_CHAR_VAL \
    BT_UUID_128_ENCODE ( 0x6e40FEC2, 0xb5a3, 0xf393, 0xe0a9, 0xe50e24dcca9e )
#define BLE_UUID_FEC_RX_CHAR BT_UUID_DECLARE_128 ( BT_UUID_FEC_RX_CHAR_VAL )
#define BT_UUID_FEC_TX_CHAR_VAL \
    BT_UUID_128_ENCODE ( 0x6e40FEC3, 0xb5a3, 0xf393, 0xe0a9, 0xe50e24dcca9e )
#define BLE_UUID_FEC_TX_CHAR BT_UUID_DECLARE_128 ( BT_UUID_FEC_TX_CHAR_VAL )

#define TX_SYNC 0xA4
#define TX_CHANNEL 0x05

typedef enum
{
    FEC_GENERAL_FE_DATA_PG = 0x10,
    FEC_GENERAL_SETTINGS_PG = 0x11,
    FEC_STATIONARY_BIKE_DATA_PG = 0x19,
    FEC_CONTROL_SET_BASIC_RESISTANCE_PG = 0x30,
    FEC_CONTROL_SET_TARGET_POWER_PG = 0x31,
    FEC_CONTROL_SET_WIND_RESISTANCE_PG = 0x32,
    FEC_CONTROL_SET_TRACK_RESISTANCE_PG = 0x33,
    FEC_REQUEST_DATA_PAGE_PG = 0x46,
    FEC_COMMAND_STATUS_PG = 0x47,
    FEC_COMMON_MANUFACTURER_IDENT_PG = 0x50,
    FEC_COMMON_PRODUCT_INFORMATION_PG = 0x51,
    FEC_COMMON_FE_CAPABILITIES_PG = 0x36,
} fec_page_t;

#define STATIONARY_BIKE 25
typedef struct __attribute__ ( ( __packed__ ) )
{
    uint8_t page;
    uint8_t equipment;
    uint8_t elapsedTime; // 0.25s
    uint8_t distance; // meters
    uint16_t speed; // 0.001 m/s
    uint8_t heartrate; // bpm
    uint8_t capabilities : 4;
    uint8_t feState : 4;
    uint8_t checksum;
} fec_general_fe_data_t;

typedef struct __attribute__ ( ( __packed__ ) )
{
    uint8_t page;
    uint16_t reserved;
    uint8_t cycleLen; // 0.01 m - 0xFF invalid
    int16_t incline; // 0.01% - 0x7FFF invalid
    uint8_t resistance; // 0.5%
    uint8_t capabilities : 4; // Reserved - 0x00
    uint8_t feState : 4;
    uint8_t checksum;
} fec_general_settings_t;

typedef struct __attribute__ ( ( __packed__ ) )
{
    uint8_t page;
    uint8_t cnt;
    uint8_t rpm;
    uint16_t wattsTotal;
    uint16_t wattsInst : 12;
    uint8_t trainStatus : 4;
    uint8_t flags : 4;
    uint8_t feState : 4;
    uint8_t checksum;
} fec_bike_data_t;

typedef struct __attribute__ ( ( __packed__ ) )
{
    uint8_t page;
    uint32_t reserved1; // 0xFF...FF
    uint16_t reserved2; // 0xFF...FF
    uint8_t resistance; // 0.5%
    uint8_t checksum;
} fec_basic_res_control_t;

typedef struct __attribute__ ( ( __packed__ ) )
{
    uint8_t page;
    uint32_t reserved1; // 0xFF...FF
    uint8_t reserved2; // 0xFF...FF
    uint16_t tgtWatts; // 0.25 watts
    uint8_t checksum;
} fec_target_pwr_control_t;

typedef struct __attribute__ ( ( __packed__ ) )
{
    uint8_t page;
    uint32_t reserved; // 0xFF...FF
    uint8_t Cw; // 0.01 kg/m - 0xFF use default
    int8_t wind_kph; // kph - 0xFF use default
    uint8_t draftFactor; // 0.01 - 0xFF use default
    uint8_t checksum;
} fec_wind_res_control_t;

typedef struct __attribute__ ( ( __packed__ ) )
{
    uint8_t page;
    uint32_t reserved; // 0xFF...FF
    int16_t incline; // 0.01% - 0xFFFF use default
    uint8_t Crr; // 0.00005 - 0xFF use default
    uint8_t checksum;
} fec_track_res_control_t;

typedef struct __attribute__ ( ( __packed__ ) )
{
    uint8_t page;
    uint16_t slaveSerial;
    uint8_t descByte1;
    uint8_t descByte2;
    uint8_t reqCnt : 7;
    uint8_t ack : 1;
    uint8_t reqPage;
    uint8_t cmdType;
    uint8_t checksum;
} fec_page_request_t;

typedef struct __attribute__ ( ( __packed__ ) )
{
    uint8_t page;
    uint8_t lastCmdId; // 0xFF indicates no control page received
    uint8_t sequenceNum; // 0xFF indicates no control page received
    uint8_t cmdStatus; // 0x00 is success
    uint8_t data[4];     // ?
    uint8_t checksum;
} fec_command_status_data_t;

typedef struct __attribute__ ( ( __packed__ ) )
{
    uint8_t page;
    uint16_t reserved; // 0xFFFF
    uint8_t hwRev;
    uint16_t manId;
    uint16_t modelNum;
    uint8_t checksum;
} fec_manufacturer_id_data_t;

typedef struct __attribute__ ( ( __packed__ ) )
{
    uint8_t page;
    uint8_t reserved; // 0xFF
    uint8_t swRevSupp; // 0xFF is invalid
    uint8_t swRev;
    uint32_t serialNum; // 0xFF...FF if no serial number
    uint8_t checksum;
} fec_product_info_data_t;

#define RES_SUPPORT_BIT ( 0 )
#define PWR_SUPPORT_BIT ( 1 )
#define SIM_SUPPORT_BIT ( 2 )
typedef struct __attribute__ ( ( __packed__ ) )
{
    uint8_t page;
    uint32_t reserved;
    uint16_t maxResistance;
    uint8_t capabilities;
    uint8_t checksum;
} fec_capabilities_t;

typedef struct __attribute__ ( ( __packed__ ) )
{
    uint8_t sync;
    uint8_t len;
    uint8_t type;
    uint8_t channel;
    uint8_t data [];  // Must include checksum at the end
} tx_msg_t;

// Functions
void fecSetTargetsCb ( set_targets_callback_t func ); 
void bt_fec_update ( bike_data_t data );

#endif  // FEC_H