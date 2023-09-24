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

#include "fec.h"

#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/types.h>
#include <zephyr/kernel.h>

#define TGT_CYCLE_MS 250
#define STACKSIZE 512
#define PRIORITY 7
#define TX_BUF_SIZE 50

// Globals
LOG_MODULE_REGISTER ( fec );
static K_SEM_DEFINE ( ble_init_ok, 1, 1 );
static bike_data_t bikeData = {};
static fec_page_request_t reqPageData = {};
static fec_command_status_data_t commandStatus
    = { 0x47, 0xFF, 0xFF, 0x00, 0x00000000, 0x00 };
static set_targets_callback_t setTargetsCbFunc = NULL;

void fecSetTargetsCb ( set_targets_callback_t func )
{
    setTargetsCbFunc = func;
}

// Config change callback
static void fec_notify_ccc_cfg_changed ( const struct bt_gatt_attr *attr,
                                         uint16_t value )
{
    bool notif_enabled = ( value == BT_GATT_CCC_NOTIFY );

    LOG_INF ( "FE-C notifications %s", notif_enabled ? "enabled" : "disabled" );
}

static uint8_t get_checksum ( uint8_t buf [], size_t len )
{
    uint8_t chkSum = 0;
    for ( int i = 0; i < len - 1; i++ ) {  // TODO - Use the first byte?
        chkSum ^= buf [i];
    }
    return chkSum;
}

static uint8_t check_checksum ( uint8_t buf [], size_t len )
{
    return buf [len - 1] - get_checksum ( buf, len );
}

static void incCmdSeq()
{
    if ( commandStatus.sequenceNum == 0xFE ) {
        commandStatus.sequenceNum = 0x00;
    } else {
        commandStatus.sequenceNum++;
    }
}

// TX callback
static ssize_t tx_cb ( struct bt_conn *conn,
                       const struct bt_gatt_attr *attr,
                       const void *buf,
                       uint16_t len,
                       uint16_t offset,
                       uint8_t flags )
{
    LOG_INF ( "TX callback, length %u", len );
    const tx_msg_t *msg = ( void * )( ( uint8_t * )buf + offset );

    if ( check_checksum ( ( void * )msg, len ) ) {
        LOG_ERR ( "Message received with bad checksum!" );
        return -1;
    } else if ( len < sizeof ( tx_msg_t ) + 2 ) {
        LOG_WRN ( "Message length too short: %u!", len );
        return len;
    }

    // First byte of every data page is page number
    const uint8_t page = msg->data [0];
    switch ( page ) {
        case FEC_CONTROL_SET_BASIC_RESISTANCE_PG:
            LOG_WRN ( "Basic resistance command received!" );
            if ( len
                 < sizeof ( tx_msg_t ) + sizeof ( fec_basic_res_control_t ) ) {
                LOG_WRN ( "Basic resistance command length too short: %u!",
                          len );
                break;
            }
            const fec_basic_res_control_t *resCtrl = ( void * )&msg->data;
            commandStatus.data [0] = 0xFF;
            commandStatus.data [1] = 0xFF;
            commandStatus.data [2] = 0xFF;
            commandStatus.data [3] = resCtrl->resistance;
            // TODO
            // setTargetsCbFunc();
            commandStatus.lastCmdId = FEC_CONTROL_SET_BASIC_RESISTANCE_PG;
            incCmdSeq();
            break;
        case FEC_CONTROL_SET_TARGET_POWER_PG:
            LOG_WRN ( "Target power command received!" );
            if ( len
                 < sizeof ( tx_msg_t ) + sizeof ( fec_target_pwr_control_t ) ) {
                LOG_WRN ( "Target power command length too short: %u!", len );
                break;
            }
            const fec_target_pwr_control_t *pwrCtrl = ( void * )&msg->data;
            commandStatus.data [0] = 0xFF;
            commandStatus.data [1] = 0xFF;
            commandStatus.data [2] = pwrCtrl->tgtWatts & 0x00FF;
            commandStatus.data [3] = pwrCtrl->tgtWatts >> 8;
            // TODO
            // setTargetsCbFunc();
            commandStatus.lastCmdId = FEC_CONTROL_SET_TARGET_POWER_PG;
            incCmdSeq();
            break;
        case FEC_CONTROL_SET_WIND_RESISTANCE_PG:
            LOG_WRN ( "Wind simulation command received!" );
            if ( len
                 < sizeof ( tx_msg_t ) + sizeof ( fec_wind_res_control_t ) ) {
                LOG_WRN ( "Wind simulation command length too short: %u!",
                          len );
                break;
            }
            const fec_wind_res_control_t *windCtrl = ( void * )&msg->data;
            commandStatus.data [0] = 0xFF;
            commandStatus.data [1] = windCtrl->Cw;
            commandStatus.data [2] = windCtrl->wind_kph;
            commandStatus.data [3] = windCtrl->draftFactor;
            // TODO
            // setTargetsCbFunc();
            commandStatus.lastCmdId = FEC_CONTROL_SET_WIND_RESISTANCE_PG;
            incCmdSeq();
            break;
        case FEC_CONTROL_SET_TRACK_RESISTANCE_PG:
            LOG_WRN ( "Track simulation command received!" );
            if ( len
                 < sizeof ( tx_msg_t ) + sizeof ( fec_track_res_control_t ) ) {
                LOG_WRN ( "Track simulation command length too short: %u!",
                          len );
                break;
            }
            const fec_track_res_control_t *trackCtrl = ( void * )&msg->data;
            commandStatus.data [0] = 0xFF;
            commandStatus.data [1] = trackCtrl->incline & 0x00FF;
            commandStatus.data [2] = trackCtrl->incline >> 8;
            commandStatus.data [3] = trackCtrl->Crr;
            // TODO
            // setTargetsCbFunc();
            commandStatus.lastCmdId = FEC_CONTROL_SET_WIND_RESISTANCE_PG;
            incCmdSeq();
            break;
        case FEC_REQUEST_DATA_PAGE_PG:
            if ( len < sizeof ( tx_msg_t ) + sizeof ( fec_page_request_t ) ) {
                LOG_WRN ( "Data page request length too short: %u!", len );
                break;
            }
            memcpy ( &reqPageData, &msg->data, sizeof ( fec_page_request_t ) );
            LOG_INF ( "Data page request: %02x!", reqPageData.reqPage );
            break;
        default:
            LOG_WRN ( "Unknown data page: %02x!", page );
    }

    return len;
}

BT_GATT_SERVICE_DEFINE (
    fec_svc,
    BT_GATT_PRIMARY_SERVICE ( BT_UUID_FEC ),
    BT_GATT_CHARACTERISTIC ( BLE_UUID_FEC_RX_CHAR,
                             BT_GATT_CHRC_NOTIFY,
                             BT_GATT_PERM_NONE,
                             NULL,
                             NULL,
                             NULL ),
    BT_GATT_CCC ( fec_notify_ccc_cfg_changed,
                  ( BT_GATT_PERM_READ | BT_GATT_PERM_WRITE ) ),
    BT_GATT_CHARACTERISTIC ( BLE_UUID_FEC_TX_CHAR,
                             BT_GATT_CHRC_WRITE
                                 | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                             BT_GATT_PERM_WRITE | BT_GATT_PERM_WRITE_ENCRYPT,
                             NULL,
                             tx_cb,
                             NULL ) );

static int fec_init ( const struct device *dev )
{
    ARG_UNUSED ( dev );

    k_sem_give ( &ble_init_ok );

    return 0;
}

void bt_fec_update ( bike_data_t data )
{
    bikeData = data;
}

static void set_checksum ( uint8_t buf [], size_t len )
{
    buf [len - 1] = get_checksum ( buf, len );
}

static size_t set_general_data_page ( void *buf )
{
    static fec_general_fe_data_t data = {};
    static const size_t msgSize
        = sizeof ( tx_msg_t ) + sizeof ( fec_general_fe_data_t );
    tx_msg_t *msg = buf;
    if ( !msg ) {
        LOG_ERR ( "set_general_data_page recieved null buffer!" );
        return 0;
    }
    msg->sync = TX_SYNC;
    msg->len = msgSize - 4;
    msg->type = 0x4E;  // TODO - BROADCAST? No idea if this is right
    msg->channel = TX_CHANNEL;

    // Update bike data
    data.page = FEC_GENERAL_FE_DATA_PG;
    data.equipment = STATIONARY_BIKE;
    data.elapsedTime += 5;  // 0.25s
    data.distance += 1;     // meters
    data.speed = 0xFFFF;    // 0.001 m/s
    data.heartrate = 0xFF;  // bpm
    data.capabilities = 0x00;
    data.feState = 0x03;  // IN_USE

    // Prep message
    memcpy ( &msg->data, &data, sizeof ( fec_general_fe_data_t ) );
    set_checksum ( ( void * )msg, msgSize );
    return msgSize;
}

static size_t set_general_settings_page ( void *buf )
{
    static fec_general_settings_t data = {};
    static const size_t msgSize
        = sizeof ( tx_msg_t ) + sizeof ( fec_general_settings_t );
    tx_msg_t *msg = buf;
    if ( !msg ) {
        LOG_ERR ( "set_general_data_page recieved null buffer!" );
        return 0;
    }
    msg->sync = TX_SYNC;
    msg->len = msgSize - 4;
    msg->type = 0x4E;  // TODO - BROADCAST? No idea if this is right
    msg->channel = TX_CHANNEL;

    // Update settings data page
    data.page = FEC_GENERAL_SETTINGS_PG;
    data.reserved = 0xFFFF;
    data.cycleLen = 0xFF;
    data.incline = 50 * ( ( int16_t )bikeData.tgt_inc - 20 );
    if ( bikeData.disp_res <= 1 ) {
        data.resistance = 0;
    } else {
        data.resistance = ( bikeData.disp_res - 1 ) * 200 / 21;
    }
    data.feState = 0x03;  // IN_USE

    // Prep message
    memcpy ( &msg->data, &data, sizeof ( fec_general_settings_t ) );
    set_checksum ( ( void * )msg, msgSize );
    return msgSize;
}

static size_t set_bike_data_page ( void *buf )
{
    static fec_bike_data_t data = {};
    static const size_t msgSize
        = sizeof ( tx_msg_t ) + sizeof ( fec_bike_data_t );
    tx_msg_t *msg = buf;
    if ( !msg ) {
        LOG_ERR ( "set_broadcast_msg recieved null buffer!" );
        return 0;
    }
    msg->sync = TX_SYNC;
    msg->len = msgSize - 4;
    msg->type = 0x4E;  // TODO - BROADCAST? No idea if this is right
    msg->channel = TX_CHANNEL;

    // Update bike data
    data.page = FEC_STATIONARY_BIKE_DATA_PG;
    data.cnt++;
    data.rpm = bikeData.act_rpm;
    data.wattsTotal += bikeData.watts;  // TODO
    data.wattsInst = bikeData.watts;
    data.trainStatus = 0x00;
    data.flags = 0x00;
    data.feState = 0x03;  // IN_USE

    // Prep message
    memcpy ( &msg->data, &data, sizeof ( fec_bike_data_t ) );
    set_checksum ( ( void * )msg, msgSize );
    return msgSize;
}

static size_t set_command_status_page ( void *buf )
{
    static const size_t msgSize
        = sizeof ( tx_msg_t ) + sizeof ( fec_command_status_data_t );
    tx_msg_t *msg = buf;
    if ( !msg ) {
        LOG_ERR ( "set_general_data_page recieved null buffer!" );
        return 0;
    }
    msg->sync = TX_SYNC;
    msg->len = msgSize - 4;
    msg->type = 0x4E;  // TODO - BROADCAST? No idea if this is right
    msg->channel = TX_CHANNEL;

    // Prep message
    memcpy ( &msg->data, &commandStatus, sizeof ( fec_command_status_data_t ) );
    set_checksum ( ( void * )msg, msgSize );
    return msgSize;
}

static size_t set_manufacturer_id_page ( void *buf )
{
    static fec_manufacturer_id_data_t data = {};
    static const size_t msgSize
        = sizeof ( tx_msg_t ) + sizeof ( fec_manufacturer_id_data_t );
    tx_msg_t *msg = buf;
    if ( !msg ) {
        LOG_ERR ( "set_general_data_page recieved null buffer!" );
        return 0;
    }
    msg->sync = TX_SYNC;
    msg->len = msgSize - 4;
    msg->type = 0x4E;  // TODO - BROADCAST? No idea if this is right
    msg->channel = TX_CHANNEL;

    // Update manufacturer id page
    data.page = FEC_COMMON_MANUFACTURER_IDENT_PG;
    data.reserved = 0xFFFF;
    data.hwRev = 0x01;    // TODO
    data.manId = 0x00FF;  // TODO - Development ID
    data.modelNum = 0x1;  // TODO

    // Prep message
    memcpy ( &msg->data, &data, sizeof ( fec_manufacturer_id_data_t ) );
    set_checksum ( ( void * )msg, msgSize );
    return msgSize;
}

static size_t set_product_info_page ( void *buf )
{
    static fec_product_info_data_t data = {};
    static const size_t msgSize
        = sizeof ( tx_msg_t ) + sizeof ( fec_product_info_data_t );
    tx_msg_t *msg = buf;
    if ( !msg ) {
        LOG_ERR ( "set_general_data_page recieved null buffer!" );
        return 0;
    }
    msg->sync = TX_SYNC;
    msg->len = msgSize - 4;
    msg->type = 0x4E;  // TODO - BROADCAST? No idea if this is right
    msg->channel = TX_CHANNEL;

    // Update product info page
    data.page = FEC_COMMON_PRODUCT_INFORMATION_PG;
    data.reserved = 0xFF;
    data.swRevSupp = 0xFF;        // Invalid
    data.swRev = 0x01;            // TODO
    data.serialNum = 0xFFFFFFFF;  // Invalid

    // Prep message
    memcpy ( &msg->data, &data, sizeof ( fec_product_info_data_t ) );
    set_checksum ( ( void * )msg, msgSize );
    return msgSize;
}

static size_t set_capabilities_page ( void *buf )
{
    static fec_capabilities_t data = {};
    static const size_t msgSize
        = sizeof ( tx_msg_t ) + sizeof ( fec_capabilities_t );
    tx_msg_t *msg = buf;
    if ( !msg ) {
        LOG_ERR ( "set_capabilities_page recieved null buffer!" );
        return 0;
    }
    msg->sync = TX_SYNC;
    msg->len = msgSize - 4;
    msg->type = 0x4E;  // TODO - BROADCAST? No idea if this is right
    msg->channel = TX_CHANNEL;

    // Update bike data
    data.page = FEC_COMMON_FE_CAPABILITIES_PG;
    data.reserved = 0xFFFFFFFF;
    data.maxResistance = 0xFFFF;
    data.capabilities = RES_SUPPORT_BIT | PWR_SUPPORT_BIT
                        | SIM_SUPPORT_BIT;  // TODO - Remove Power?

    // Prep message
    memcpy ( &msg->data, &data, sizeof ( fec_capabilities_t ) );
    set_checksum ( ( void * )msg, msgSize );
    return msgSize;
}

static void send_pacer()
{
    static uint32_t last_ms = 0;
    int64_t elapsed_ms = k_uptime_get_32() - last_ms;
    if ( elapsed_ms < 0 ) {
        elapsed_ms += 1LL << 32;
    }
    if ( elapsed_ms < TGT_CYCLE_MS ) {
        k_msleep ( TGT_CYCLE_MS - elapsed_ms );
    }
    last_ms = k_uptime_get_32();
}

static void send_msg ( void *buf, size_t len )
{
    send_pacer();
    /*uint8_t dataPage;
    memcpy ( &dataPage, ( uint8_t * )buf + 4, sizeof ( uint8_t ) );
    LOG_INF ( "Transmitting data page: %02x", dataPage );*/
    int rc = bt_gatt_notify_uuid ( NULL,
                                   BLE_UUID_FEC_RX_CHAR,
                                   fec_svc.attrs,
                                   buf,
                                   len );
    if ( rc == -ENOTCONN ) {
        // Do nothing
    } else if ( rc < 0 ) {
        LOG_ERR ( "Failed to transmit message, error: %d", rc );
    }
}

static void bt_fec_notify()
{
    static uint8_t buf [TX_BUF_SIZE] = {};
    int len = 0;
    static int cnt = 0;
    cnt++;

    // First check for requests
    for ( int i = 0; i < reqPageData.reqCnt; i++ ) {
        switch ( reqPageData.reqPage ) {
            case 0:
                // Don't do anything, no request
                break;
            case FEC_GENERAL_FE_DATA_PG:
                len = set_general_data_page ( &buf );
                send_msg ( buf, len );
                break;
            case FEC_GENERAL_SETTINGS_PG:
                len = set_general_settings_page ( &buf );
                send_msg ( buf, len );
                break;
            case FEC_STATIONARY_BIKE_DATA_PG:
                len = set_bike_data_page ( &buf );
                send_msg ( buf, len );
                break;
            case FEC_COMMAND_STATUS_PG:
                len = set_command_status_page ( &buf );
                send_msg ( buf, len );
                break;
            case FEC_COMMON_MANUFACTURER_IDENT_PG:
                len = set_manufacturer_id_page ( &buf );
                send_msg ( buf, len );
                break;
            case FEC_COMMON_PRODUCT_INFORMATION_PG:
                len = set_product_info_page ( &buf );
                send_msg ( buf, len );
                break;
            case FEC_COMMON_FE_CAPABILITIES_PG:
                len = set_capabilities_page ( &buf );
                send_msg ( buf, len );
                break;
            default:
                LOG_WRN ( "Unknown data page request: %02x!",
                          reqPageData.reqPage );
        }
    }
    memset ( &reqPageData, 0, sizeof ( fec_page_request_t ) );

    /*
     * 10.1.1 - Minimum Data Page Requirements
     *     FEC_GENERAL_FE_DATA_PG: 2 Hz - 2x (consecutive) per sec or every 5th
     *     FEC_GENERAL_SETTINGS_PG = 0.2 Hz - At least once every 20 messages
     *     FEC_STATIONARY_BIKE_DATA_PG: 0.8 Hz - At least once every 5 messages
     *     FEC_COMMAND_STATUS_PG: On request
     *     FEC_COMMON_MANUFACTURER_IDENT_PG: 2x (consecutive) every 132 messages
     *     FEC_COMMON_PRODUCT_INFORMATION_PG: 2x (consecutive) every 132
     *         messages
     *     FEC_COMMON_FE_CAPABILITIES_PG: On request
     */
    if ( cnt % 132 == 0 ) {
        len = set_manufacturer_id_page ( &buf );
        send_msg ( buf, len );
        cnt++;
        len = set_manufacturer_id_page ( &buf );
        send_msg ( buf, len );
        cnt++;
        len = set_product_info_page ( &buf );
        send_msg ( buf, len );
        cnt++;
        len = set_product_info_page ( &buf );
        send_msg ( buf, len );
    } else if ( cnt % 20 == 2 ) {
        len = set_general_settings_page ( &buf );
        send_msg ( buf, len );
    } else if ( cnt % 5 == 0 ) {
        len = set_general_data_page ( &buf );
        send_msg ( buf, len );
        cnt++;
        len = set_general_data_page ( &buf );
        send_msg ( buf, len );
    } else {
        len = set_bike_data_page ( &buf );
        send_msg ( buf, len );
    }
}

static void ble_fec_notify_thread ( void )
{
    k_sem_take ( &ble_init_ok, K_FOREVER );

    for ( ;; ) {
        bt_fec_notify();
    }
}

/*K_THREAD_DEFINE ( ble_fec_notify_thread_id,
                  STACKSIZE,
                  ble_fec_notify_thread,
                  NULL,
                  NULL,
                  NULL,
                  PRIORITY,
                  0,
                  5000 );

SYS_INIT ( fec_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY );*/
