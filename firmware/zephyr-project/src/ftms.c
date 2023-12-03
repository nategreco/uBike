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

#include "ftms.h"

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

#define SEM_TIMEOUT K_MSEC ( 500 )

LOG_MODULE_REGISTER ( ftms );
static set_targets_callback_t setTargetsCbFunc = NULL;
static bool ftms_bike_notify = false;
static bool ftms_status_notify = false;
static bool ftms_control_notify = false;
K_SEM_DEFINE ( ftms_sem, 0, 1 );

void ftmsSetTargetsCb ( set_targets_callback_t func )
{
    setTargetsCbFunc = func;
}

// Config change callback
static void ftms_bike_data_ccc_changed ( const struct bt_gatt_attr *attr,
                                         uint16_t value )
{
    ftms_bike_notify = ( value == BT_GATT_CCC_NOTIFY );

    LOG_INF ( "FTMS bike data notifications %s",
              ftms_bike_notify ? "enabled" : "disabled" );
}

static void ftms_status_ccc_changed ( const struct bt_gatt_attr *attr,
                                      uint16_t value )
{
    ftms_status_notify = ( value == BT_GATT_CCC_NOTIFY );

    LOG_INF ( "FTMS status notifications %s",
              ftms_status_notify ? "enabled" : "disabled" );
}

static void ftms_control_ccc_changed ( const struct bt_gatt_attr *attr,
                                       uint16_t value )
{
    ftms_control_notify = ( value == BT_GATT_CCC_NOTIFY );

    LOG_INF ( "FTMS control point notifications %s",
              ftms_control_notify ? "enabled" : "disabled" );
}

static ble_ftms_features_t ftms_features;
static ssize_t read_feat ( struct bt_conn *conn,
                           const struct bt_gatt_attr *attr,
                           void *buf,
                           uint16_t len,
                           uint16_t offset )
{
    return bt_gatt_attr_read ( conn,
                               attr,
                               buf,
                               len,
                               offset,
                               &ftms_features,
                               sizeof ( ftms_features ) );
}

static ble_ftms_inclination_range_data_t inc_range_data;
static ssize_t read_inc_range ( struct bt_conn *conn,
                                const struct bt_gatt_attr *attr,
                                void *buf,
                                uint16_t len,
                                uint16_t offset )
{
    return bt_gatt_attr_read ( conn,
                               attr,
                               buf,
                               len,
                               offset,
                               &inc_range_data,
                               sizeof ( inc_range_data ) );
}

static ble_ftms_resistance_range_data_t res_range_data;
static ssize_t read_res_range ( struct bt_conn *conn,
                                const struct bt_gatt_attr *attr,
                                void *buf,
                                uint16_t len,
                                uint16_t offset )
{
    return bt_gatt_attr_read ( conn,
                               attr,
                               buf,
                               len,
                               offset,
                               &res_range_data,
                               sizeof ( res_range_data ) );
}

static void control_response ( struct bt_conn *conn,
                               const struct bt_gatt_attr *attr,
                               uint8_t req_op,
                               const void *data,
                               uint16_t data_len );

static ssize_t write_control ( struct bt_conn *conn,
                               const struct bt_gatt_attr *attr,
                               const void *buf,
                               uint16_t len,
                               uint16_t offset,
                               uint8_t flags )
{
    // Lock semaphore to prevent notifications before response
    if ( k_sem_take ( &ftms_sem, SEM_TIMEOUT ) ) {
        LOG_WRN ( "Failed to get FTMS semaphore!  Continuing anyways..." );
    }

    const ctrl_point_req_t *req = buf;
    if ( offset ) {
        k_sem_give ( &ftms_sem );
        return BT_GATT_ERR ( BT_ATT_ERR_INVALID_OFFSET );
    }
    const uint16_t data_len = len - sizeof ( ctrl_point_req_t );

    switch ( req->req_op ) {
        case OPCODE_REQUEST:
        case OPCODE_RESET:
        case OPCODE_SET_INC:
        case OPCODE_SET_RES:
        case OPCODE_START:
            control_response ( conn, attr, req->req_op, &req->param, data_len );
            break;
        case OPCODE_SIM_PARAMS: {
            if ( data_len != sizeof ( sim_data_param_t ) ) {
                LOG_ERR ( "Wrong length for bike sim parameters!\n" );
                break;
            }
            sim_data_param_t *sim_data = ( void * )&req->param;
            /*LOG_INF ( "Params: %d,%d,%u,%u",
                      sim_data->wind_mps,
                      sim_data->grade_hundredths_pct,
                      sim_data->Crr,
                      sim_data->Cw );*/
            bike_tgts_t tgts = { sim_data->grade_hundredths_pct, 0xFF };
            if ( setTargetsCbFunc ) {
                setTargetsCbFunc ( tgts );
            } else {
                LOG_ERR ( "Bike target callback not registered!" );
            }
            control_response ( conn, attr, req->req_op, &req->param, data_len );
            break;
        }
        default:
            LOG_WRN ( "Unknown opcode: %x!\n", req->req_op );
            // TODO - Return failure
            control_response ( conn, attr, req->req_op, &req->param, data_len );
    }

    // Give up semaphore
    k_sem_give ( &ftms_sem );

    return len;
}

// Create service
BT_GATT_SERVICE_DEFINE (
    ftms_svc,
    BT_GATT_PRIMARY_SERVICE ( BT_UUID_FTMS ),
    BT_GATT_CHARACTERISTIC ( BLE_UUID_FTMS_FEATURE_CHAR,
                             BT_GATT_CHRC_READ,
                             BT_GATT_PERM_READ,
                             read_feat,
                             NULL,
                             NULL ),
    BT_GATT_CHARACTERISTIC ( BLE_UUID_INDOOR_BIKE_DATA_CHAR,
                             BT_GATT_CHRC_NOTIFY,
                             BT_GATT_PERM_NONE,
                             NULL,
                             NULL,
                             NULL ),
    BT_GATT_CCC ( ftms_bike_data_ccc_changed,
                  ( BT_GATT_PERM_READ | BT_GATT_PERM_WRITE ) ),
    BT_GATT_CHARACTERISTIC ( BLUE_UUID_SUPPORTED_INCLINATION_RANGE_CHAR,
                             BT_GATT_CHRC_READ,
                             BT_GATT_PERM_READ,
                             read_inc_range,
                             NULL,
                             NULL ),
    BT_GATT_CHARACTERISTIC ( BLUE_UUID_SUPPORTED_RESISTANCE_RANGE_CHAR,
                             BT_GATT_CHRC_READ,
                             BT_GATT_PERM_READ,
                             read_res_range,
                             NULL,
                             NULL ),
    BT_GATT_CHARACTERISTIC ( BLUE_UUID_FITNESS_CONTROL_POINT_CHAR,
                             BT_GATT_CHRC_WRITE | BT_GATT_CHRC_NOTIFY,
                             BT_GATT_PERM_WRITE,
                             NULL,
                             write_control,
                             NULL ),
    BT_GATT_CCC ( ftms_control_ccc_changed,
                  ( BT_GATT_PERM_READ | BT_GATT_PERM_WRITE ) ),
    BT_GATT_CHARACTERISTIC ( BLE_UUID_FTMS_STATUS_CHAR,
                             BT_GATT_CHRC_NOTIFY,
                             BT_GATT_PERM_NONE,
                             NULL,
                             NULL,
                             NULL ),
    BT_GATT_CCC ( ftms_status_ccc_changed,
                  ( BT_GATT_PERM_READ | BT_GATT_PERM_WRITE ) ), );

static void control_response ( struct bt_conn *conn,
                               const struct bt_gatt_attr *attr,
                               uint8_t req_op,
                               const void *data,
                               uint16_t data_len )
{
    if ( !ftms_bike_notify ) {
        LOG_ERR ( "Control point notifications not enabled!" );
        return;
    }

    ctrl_point_resp_t *resp;
    uint8_t buf [sizeof ( ctrl_point_resp_t ) + data_len];

    resp = ( void * )buf;
    resp->resp_op = OPCODE_RESPONSE;
    resp->req_op = req_op;
    resp->result = OPCODE_SUCCESS;
    if ( data && data_len ) {
        memcpy ( &resp->param, data, data_len );
    }
    bt_gatt_notify_uuid ( conn,
                          BLUE_UUID_FITNESS_CONTROL_POINT_CHAR,
                          attr,
                          buf,
                          sizeof ( buf ) );
}

static int ftms_init ( const struct device *dev )
{
    ARG_UNUSED ( dev );

    memset ( &ftms_features, 0, sizeof ( ftms_features ) );
    ftms_features.feat_blsc
        = BLE_FTMS_FEATURE_CADENCE_SUPPORTED_BIT
          | BLE_FTMS_FEATURE_POWER_MEASUREMENT_SUPPORTED_BIT;

    ftms_features.tgt_blsc = BLE_FTMS_TARGET_INCLINATION_SUPPORTED_BIT
                             | BLE_FTMS_TARGET_RESISTANCE_SUPPORTED_BIT
                             | BLE_FTMS_BIKE_SIMULATION_SUPPORTED_BIT;

    inc_range_data.inc_tenth_pct = 5;
    inc_range_data.max_tenth_pct = 200;
    inc_range_data.min_tenth_pct = -100;

    res_range_data.inc_cnt = 1;
    res_range_data.max_cnt = 22;
    res_range_data.min_cnt = 1;

    LOG_INF ( "FTMS initialized" );

    return 0;
}

int bt_ftms_bike_notify ( bike_data_t bikeData )
{
    if ( !ftms_bike_notify ) {
        return -EACCES;
    }

    // Lock semaphore
    if ( k_sem_take ( &ftms_sem, SEM_TIMEOUT ) ) {
        LOG_WRN ( "Failed to get FTMS semaphore!  Continuing anyways..." );
    }

    static ble_ftms_indoor_bike_data_t data = {};
    data.flags = BLE_FTMS_INDOOR_FLAGS_FIELD_INSTANTANEOUS_CADENCE_PRESENT
                 | BLE_FTMS_INDOOR_FLAGS_FIELD_INSTANTANEOUS_POWER_PRESENT;
    data.InstantaneousCadence = 2 * bikeData.act_rpm;
    data.InstantaneousPower = bikeData.watts;

    int rc;
    rc = bt_gatt_notify_uuid ( NULL,
                               BLE_UUID_INDOOR_BIKE_DATA_CHAR,
                               ftms_svc.attrs,
                               &data,
                               sizeof ( data ) );

    // Give up semaphore
    k_sem_give ( &ftms_sem );

    return rc == -ENOTCONN ? 0 : rc;
}

int bt_ftms_status_notify()
{
    if ( !ftms_status_notify ) {
        return -EACCES;
    }

    // Lock semaphore
    if ( k_sem_take ( &ftms_sem, SEM_TIMEOUT ) ) {
        LOG_WRN ( "Failed to get FTMS semaphore!  Continuing anyways..." );
    }

    int rc;
    ftms_status_t status = {};
    status.op = OPCODE_STARTED;

    rc = bt_gatt_notify_uuid ( NULL,
                               BLE_UUID_FTMS_STATUS_CHAR,
                               ftms_svc.attrs,
                               &status,
                               sizeof ( status ) );  // TODO - Size is wrong!

    // Give up semaphore
    k_sem_give ( &ftms_sem );

    return rc == -ENOTCONN ? 0 : rc;
}

SYS_INIT ( ftms_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY );
