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

#include "cscs.h"

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

LOG_MODULE_REGISTER ( cscs );

// Read feature callback
static ble_cscs_features_t cscs_features = {};
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
                               &cscs_features,
                               sizeof ( cscs_features ) );
}

static void cscs_ccc_cfg_changed ( const struct bt_gatt_attr *attr,
                                   uint16_t value )
{
    ARG_UNUSED ( attr );

    bool notif_enabled = ( value == BT_GATT_CCC_NOTIFY );

    LOG_INF ( "CSCS notifications %s", notif_enabled ? "enabled" : "disabled" );
}

BT_GATT_SERVICE_DEFINE (
    cscs_svc,
    BT_GATT_PRIMARY_SERVICE ( BT_UUID_CSC ),
    BT_GATT_CHARACTERISTIC ( BLE_UUID_CSCS_FEATURE_CHAR,
                             BT_GATT_CHRC_READ,
                             BT_GATT_PERM_READ,
                             read_feat,
                             NULL,
                             NULL ),
    BT_GATT_CHARACTERISTIC ( BLE_UUID_CSCS_MEASUREMENT_CHAR,
                             BT_GATT_CHRC_NOTIFY,
                             BT_GATT_PERM_NONE,
                             NULL,
                             NULL,
                             NULL ),
    BT_GATT_CCC ( NULL,
                  ( BT_GATT_PERM_READ | BT_GATT_PERM_WRITE ) ) );

static int cscs_init ( const struct device *dev )
{
    ARG_UNUSED ( dev );

    cscs_features.feat_blsc = BLE_CSCS_FEATURE_CRANK_REV;

    return 0;
}

static void set_crank_data ( uint16_t rpm, ble_cscs_measurement_data_t *data )
{
    data->flags = BLE_CSCS_CRANK_FLAGS_FIELD;

    // Handle first call/zero rpm
    static uint32_t lastRev_ms = 0;
    if ( !lastRev_ms || !rpm ) {
        lastRev_ms = k_uptime_get_32();
        return;
    }

    // Get elapsed time since last revolution
    int64_t elapsed_ms = k_uptime_get_32() - lastRev_ms;
    if ( elapsed_ms < 0 ) {
        elapsed_ms += 1LL << 32;
    }

    // Compute data
    int64_t elapsed_revs = ( elapsed_ms * rpm ) / 60000LL;
    data->totalRevs_cnt += elapsed_revs;
    lastRev_ms += ( 60000LL * elapsed_revs ) / rpm;
    data->lastCrank_1024 = ( lastRev_ms * 128LL ) / 125LL;
}

int bt_cscs_bike_notify ( bike_data_t bikeData )
{
    static ble_cscs_measurement_data_t data = {};
    set_crank_data ( bikeData.act_rpm, &data );

    int rc = bt_gatt_notify_uuid ( NULL,
                                   BLE_UUID_CSCS_MEASUREMENT_CHAR,
                                   cscs_svc.attrs,
                                   &data,
                                   sizeof ( data ) );
    return rc == -ENOTCONN ? 0 : rc;
}

SYS_INIT ( cscs_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY );