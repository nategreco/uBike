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

#include "cps.h"

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

LOG_MODULE_REGISTER ( cps );
static bool notify_enabled = false;

// Read feature callback
static ble_cps_features_t cps_features = {};
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
                               &cps_features,
                               sizeof ( cps_features ) );
}

static void cps_ccc_cfg_changed ( const struct bt_gatt_attr *attr,
                                  uint16_t value )
{
    ARG_UNUSED ( attr );

    notify_enabled = ( value == BT_GATT_CCC_NOTIFY );

    LOG_INF ( "CPS notifications %s", notify_enabled ? "enabled" : "disabled" );
}

BT_GATT_SERVICE_DEFINE (
    cps_svc,
    BT_GATT_PRIMARY_SERVICE ( BT_UUID_CPS ),
    BT_GATT_CHARACTERISTIC ( BLE_UUID_CYCLING_POWER_FEATURE_CHAR,
                             BT_GATT_CHRC_READ,
                             BT_GATT_PERM_READ,
                             read_feat,
                             NULL,
                             NULL ),
    BT_GATT_CHARACTERISTIC ( BLE_UUID_CYCLING_POWER_MEASUREMENT_CHAR,
                             BT_GATT_CHRC_NOTIFY,
                             BT_GATT_PERM_NONE,
                             NULL,
                             NULL,
                             NULL ),
    BT_GATT_CCC ( cps_ccc_cfg_changed,
                  ( BT_GATT_PERM_READ | BT_GATT_PERM_WRITE ) ) );

static int cps_init ( const struct device *dev )
{
    ARG_UNUSED ( dev );

    /// cps_features doesn't use any optional flags

    return 0;
}

int bt_cps_notify ( bike_data_t bikeData )
{
    if ( !notify_enabled ) {
        return -EACCES;
    }

    static ble_cps_measurement_data_t data = {};
    data.InstantaneousPower = bikeData.watts;

    int rc = bt_gatt_notify_uuid ( NULL,
                                   BLE_UUID_CYCLING_POWER_MEASUREMENT_CHAR,
                                   cps_svc.attrs,
                                   &data,
                                   sizeof ( data ) );
    return rc == -ENOTCONN ? 0 : rc;
}

SYS_INIT ( cps_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY );
