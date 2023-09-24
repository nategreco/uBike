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
#include <zephyr/logging/log.h>
#include <zephyr/types.h>
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER ( ftms );
static set_targets_callback_t setTargetsCbFunc = NULL;

void ftmsSetTargetsCb ( set_targets_callback_t func )
{
    setTargetsCbFunc = func;
}

static inline int bt_gatt_indicate_uuid ( struct bt_conn *conn,
                                          const struct bt_uuid *uuid,
                                          const struct bt_gatt_attr *attr,
                                          const void *data,
                                          uint16_t len )
{
    struct bt_gatt_indicate_params params;
    memset ( &params, 0, sizeof ( params ) );

    params.uuid = uuid;
    params.attr = attr;
    params.data = data;
    params.len = len;

    return bt_gatt_indicate ( conn, &params );
}

// Config change callback
static void ftms_notify_ccc_cfg_changed ( const struct bt_gatt_attr *attr,
                                          uint16_t value )
{
    bool notif_enabled = ( value == BT_GATT_CCC_NOTIFY );

    LOG_INF ( "FTMS notifications %s", notif_enabled ? "enabled" : "disabled" );
}

static void ftms_indicate_ccc_cfg_changed ( const struct bt_gatt_attr *attr,
                                            uint16_t value )
{
    bool indic_enabled = ( value == BT_GATT_CCC_INDICATE );

    LOG_INF ( "FTMS indications %s", indic_enabled ? "enabled" : "disabled" );
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
    const ctrl_point_req_t *req = buf;
    if ( offset ) {
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
    BT_GATT_CCC ( ftms_notify_ccc_cfg_changed,
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
                             BT_GATT_CHRC_WRITE | BT_GATT_CHRC_INDICATE,
                             BT_GATT_PERM_WRITE | BT_GATT_PERM_WRITE_ENCRYPT,
                             NULL,
                             write_control,
                             NULL ),
    BT_GATT_CCC ( NULL,
                  ( BT_GATT_PERM_READ | BT_GATT_PERM_WRITE ) ),
    BT_GATT_CHARACTERISTIC ( BLE_UUID_FTMS_STATUS_CHAR,
                             BT_GATT_CHRC_NOTIFY,
                             BT_GATT_PERM_NONE,
                             NULL,
                             NULL,
                             NULL ),
    BT_GATT_CCC ( NULL,
                  ( BT_GATT_PERM_READ | BT_GATT_PERM_WRITE ) ), );

static void control_response ( struct bt_conn *conn,
                               const struct bt_gatt_attr *attr,
                               uint8_t req_op,
                               const void *data,
                               uint16_t data_len )
{
    ctrl_point_resp_t *resp;
    uint8_t buf [sizeof ( ctrl_point_resp_t ) + data_len];

    resp = ( void * )buf;
    resp->resp_op = OPCODE_RESPONSE;
    resp->req_op = req_op;
    resp->result = OPCODE_SUCCESS;
    if ( data && data_len ) {
        memcpy ( &resp->param, data, data_len );
    }

    bt_gatt_indicate_uuid ( conn,
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
    return rc == -ENOTCONN ? 0 : rc;
}

int bt_ftms_status_notify()
{
    int rc;
    ftms_status_t status = {};
    status.op = OPCODE_STARTED;

    rc = bt_gatt_notify_uuid ( NULL,
                               BLE_UUID_FTMS_STATUS_CHAR,
                               ftms_svc.attrs,
                               &status,
                               sizeof ( status ) );  // TODO - Size is wrong!

    return rc == -ENOTCONN ? 0 : rc;
}

SYS_INIT ( ftms_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY );
