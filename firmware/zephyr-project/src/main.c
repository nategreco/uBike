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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/drivers/counter.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/mgmt/mcumgr/transport/smp_bt.h>
#include <zephyr/random/rand32.h>
#include <zephyr/types.h>

#include "asciiModbus.h"
#include "bikeControl.h"
#include "cps.h"
#include "cscs.h"
#include "display.h"
// #include "fec.h"
#include "ftms.h"
#include "version.h"

LOG_MODULE_REGISTER ( app );

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN ( sizeof ( DEVICE_NAME ) - 1 )
#define BT_DEVICE_CYCLING_APPEARANCE 0x012

#define TGT_CYCLE_MS 500

#define LED0_NODE DT_ALIAS ( led0 )
#define RS485DE_NODE DT_ALIAS ( rs485de )
#define LCD_RST_NODE DT_ALIAS ( lcdrst )
#define CPT_RST_NODE DT_ALIAS ( cptrst )

#define BUT1_NODE DT_ALIAS ( addinc )
#define BUT2_NODE DT_ALIAS ( subinc )
#define BUT3_NODE DT_ALIAS ( addres )
#define BUT4_NODE DT_ALIAS ( subres )

#define INC_ALARM_ID 0
#define RES_ALARM_ID 1

#define RX_BUFF_SIZE 100
#define TX_BUFF_SIZE 20
#define RX_TIMEOUT_US 2000
#define TX_TIMEOUT_US 2000
#define SEM_TIMEOUT K_MSEC ( 50 )

static const struct bt_data ad []
    = { BT_DATA_BYTES ( BT_DATA_GAP_APPEARANCE,
                        ( BT_DEVICE_CYCLING_APPEARANCE >> 0 ) & 0xff,
                        ( BT_DEVICE_CYCLING_APPEARANCE >> 8 ) & 0xff ),
        BT_DATA_BYTES ( BT_DATA_FLAGS,
                        ( BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR ) ),
        BT_DATA_BYTES ( BT_DATA_UUID16_ALL,
                        BT_UUID_16_ENCODE ( BT_UUID_CPS_VAL ),
                        BT_UUID_16_ENCODE ( BT_UUID_CSC_VAL ),
                        BT_UUID_16_ENCODE ( BT_UUID_FTMS_VAL ) ),
        BT_DATA ( BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN ) };

static const struct bt_data sd [] = {
    BT_DATA_BYTES ( BT_DATA_UUID128_ALL,
                    0x84,
                    0xaa,
                    0x60,
                    0x74,
                    0x52,
                    0x8a,
                    0x8b,
                    0x86,
                    0xd3,
                    0x4c,
                    0xb7,
                    0x1d,
                    0x1d,
                    0xdc,
                    0x53,
                    0x8d ),
};

static void adv_start ( void )
{
    int err;

    err = bt_le_adv_start ( BT_LE_ADV_CONN,
                            ad,
                            ARRAY_SIZE ( ad ),
                            sd,
                            ARRAY_SIZE ( sd ) );
    if ( err ) {
        LOG_ERR ( "Advertising failed to start (err %d)", err );
        return;
    }

    LOG_INF ( "Advertising successfully started" );
}

static void connected ( struct bt_conn *conn, uint8_t err )
{
    if ( err ) {
        LOG_ERR ( "Connection failed (err 0x%02x)", err );
    } else {
        LOG_INF ( "Connected" );
    }
}

static void disconnected ( struct bt_conn *conn, uint8_t reason )
{
    LOG_INF ( "Disconnected (reason 0x%02x)", reason );
}

BT_CONN_CB_DEFINE ( conn_callbacks )
    = { .connected = connected, .disconnected = disconnected };

K_SEM_DEFINE ( rs485_sem, 0, 1 );
K_CONDVAR_DEFINE ( txDoneVar );
K_CONDVAR_DEFINE ( rxDoneVar );
K_MUTEX_DEFINE ( mutex );
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET ( LED0_NODE, gpios );
static const struct gpio_dt_spec rs485de
    = GPIO_DT_SPEC_GET ( RS485DE_NODE, gpios );
static const struct gpio_dt_spec lcdRst
    = GPIO_DT_SPEC_GET ( LCD_RST_NODE, gpios );
static const struct gpio_dt_spec cptRst
    = GPIO_DT_SPEC_GET ( CPT_RST_NODE, gpios );
static const struct device *uart = DEVICE_DT_GET ( DT_NODELABEL ( uart1 ) );
static const struct gpio_dt_spec addInc = GPIO_DT_SPEC_GET ( BUT1_NODE, gpios );
static const struct gpio_dt_spec subInc = GPIO_DT_SPEC_GET ( BUT3_NODE, gpios );
static const struct gpio_dt_spec addRes = GPIO_DT_SPEC_GET ( BUT2_NODE, gpios );
static const struct gpio_dt_spec subRes = GPIO_DT_SPEC_GET ( BUT4_NODE, gpios );
static const struct device *rtc2_dev = DEVICE_DT_GET ( DT_NODELABEL ( rtc0 ) );
struct counter_alarm_cfg alarmCfg;
static struct gpio_callback addIncCbData;
static struct gpio_callback subIncCbData;
static struct gpio_callback addResCbData;
static struct gpio_callback subResCbData;
static uint8_t rx_buf_1 [RX_BUFF_SIZE] = { 0 };
static uint8_t rx_buf_2 [RX_BUFF_SIZE] = { 0 };
static uint8_t rx_buf_num = 1;
static uint8_t tx_buf [TX_BUFF_SIZE] = { 0 };

int send_cmd ( cmd_msg_data_t cmd )
{
#if defined( CONFIG_BOARD_NRF52840DK_NRF52840 ) \
    || defined( CONFIG_BOARD_NRF52840DONGLE_NRF52840 ) \
    || defined ( CONFIG_BOARD_NRF5340DK_NRF5340_CPUAPP_NS )
    return -1;
#endif

    // Check the semaphore isn't taken
    k_mutex_lock ( &mutex, K_FOREVER );
    if ( k_sem_take ( &rs485_sem, SEM_TIMEOUT ) ) {
        LOG_WRN ( "Failed to get rs485 semaphore!  Transmitting anyways..." );
    }

    // Send the message
    gpio_pin_set_dt ( &rs485de, 1 );
    k_msleep ( 5 );  // Give transciever time to update
    if ( uart_tx ( uart, tx_buf, create_msg ( tx_buf, cmd ), TX_TIMEOUT_US ) ) {
        gpio_pin_set_dt ( &rs485de, 0 );
        k_sem_give ( &rs485_sem );
        LOG_ERR ( "Failed to send message..." );
        return -1;
    }

    // Wait for send
    if ( k_condvar_wait ( &txDoneVar, &mutex, SEM_TIMEOUT ) ) {
        LOG_ERR ( "Timed out waiting for TX done." );
        return -2;
    }
    k_mutex_unlock ( &mutex );

    // Wait for reply
    if ( k_condvar_wait ( &rxDoneVar, &mutex, SEM_TIMEOUT ) ) {
        LOG_ERR ( "Timed out waiting for reply." );
        return -3;
    }
    k_mutex_unlock ( &mutex );

    return 0;
}

static void counter_interrupt_cb ( const struct device *counter_dev,
                                   uint8_t chan_id,
                                   uint32_t ticks,
                                   void *user_data )
{
    buttonStatus_t adj;
    uint64_t delay_us;
    if ( chan_id == INC_ALARM_ID ) {
        adj = evaluateButton ( gpio_pin_get ( addInc.port, addInc.pin ),
                               gpio_pin_get ( subInc.port, subInc.pin ) );
        if ( adj == NOTHING ) {
            return;
        }
        adjustIncline ( adj );
        delay_us = INC_BUTTON_DLY_US;
    } else if ( chan_id == RES_ALARM_ID ) {
        adj = evaluateButton ( gpio_pin_get ( addRes.port, addRes.pin ),
                               gpio_pin_get ( subRes.port, subRes.pin ) );
        if ( adj == NOTHING ) {
            return;
        }
        adjustResistance ( adj );
        delay_us = RES_BUTTON_DLY_US;
    } else {
        LOG_ERR ( "Invalid channel id: %d!", chan_id );
        return;
    }

    // Restart
    alarmCfg.ticks = counter_us_to_ticks ( rtc2_dev, delay_us );
    int err = counter_set_channel_alarm ( counter_dev, chan_id, &alarmCfg );
    if ( err ) {
        LOG_ERR ( "Alarm %d could not be set! Error: %d", chan_id, err );
    }
}

static void incPressed ( const struct device *dev,
                         struct gpio_callback *cb,
                         uint32_t pins )
{
    alarmCfg.ticks = counter_us_to_ticks ( rtc2_dev, INC_BUTTON_DLY_US );
    int err = counter_set_channel_alarm ( rtc2_dev, INC_ALARM_ID, &alarmCfg );
    if ( err ) {
        LOG_ERR ( "Alarm %d could not be set! Error: %d", INC_ALARM_ID, err );
    } else {
        // Setting counter fails if its already been set, which is often the
        // case when there is bounce in the signal.  So only increment if
        // setting the timer does not fail, this implements a psuedo-debounce.
        buttonStatus_t adj
            = evaluateButton ( gpio_pin_get ( addInc.port, addInc.pin ),
                               gpio_pin_get ( subInc.port, subInc.pin ) );
        adjustIncline ( adj );
    }
}

static void resPressed ( const struct device *dev,
                         struct gpio_callback *cb,
                         uint32_t pins )
{
    alarmCfg.ticks = counter_us_to_ticks ( rtc2_dev, RES_BUTTON_DLY_US );
    int err = counter_set_channel_alarm ( rtc2_dev, RES_ALARM_ID, &alarmCfg );
    if ( err ) {
        LOG_ERR ( "Alarm %d could not be set! Error: %d", RES_ALARM_ID, err );
    } else {
        // Setting counter fails if its already been set, which is often the
        // case when there is bounce in the signal.  So only increment if
        // setting the timer does not fail, this implements a psuedo-debounce.
        buttonStatus_t adj
            = evaluateButton ( gpio_pin_get ( addRes.port, addRes.pin ),
                               gpio_pin_get ( subRes.port, subRes.pin ) );
        adjustResistance ( adj );
    }
}

static void add_rx_bytes ( char *buff, size_t offset, size_t len )
{
    static char msg [2 * RX_BUFF_SIZE] = { 0 };
    static size_t pos = 0;
    for ( int i = 0; i < len; i++ ) {
        if ( pos + i >= 2 * RX_BUFF_SIZE ) {
            LOG_ERR ( "Message buffer overflow!" );
            pos = 0;
            memset ( msg, 0, 2 * RX_BUFF_SIZE );
            return;
        }
        msg [pos] = buff [offset + i];
        if ( msg [pos] == TERM_CHAR_8 ) {
            char *start = strrchr ( msg, START_CHAR_8 );
            if ( start ) {
                if ( start != msg ) {
                    LOG_WRN ( "Data thrown out!" );
                }
                int res = new_msg ( start, pos - ( start - msg ) );
                if ( res ) {
                    LOG_ERR ( "Failed to process new message: %d", res );
                }
                k_sem_give ( &rs485_sem );
                k_condvar_signal ( &rxDoneVar );
            }
            pos = 0;
            memset ( msg, 0, 2 * RX_BUFF_SIZE );
        } else {
            pos++;
        }
    }
}

static int enable_rx()
{
    if ( rx_buf_num == 2 ) {
        rx_buf_num = 1;
        return uart_rx_enable ( uart, rx_buf_1, RX_BUFF_SIZE, RX_TIMEOUT_US );
    }
    rx_buf_num = 2;
    return uart_rx_enable ( uart, rx_buf_2, RX_BUFF_SIZE, RX_TIMEOUT_US );
}

static int switch_rx_buf()
{
    if ( rx_buf_num == 2 ) {
        rx_buf_num = 1;
        return uart_rx_buf_rsp ( uart, rx_buf_1, RX_BUFF_SIZE );
    }
    rx_buf_num = 2;
    return uart_rx_buf_rsp ( uart, rx_buf_2, RX_BUFF_SIZE );
}

static void release_rx_buf()
{
    if ( rx_buf_num == 2 ) {
        memset ( rx_buf_1, 0, RX_BUFF_SIZE );
    }
    memset ( rx_buf_2, 0, RX_BUFF_SIZE );
}

static void uart_cb ( const struct device *dev,
                      struct uart_event *evt,
                      void *user_data )
{
    switch ( evt->type ) {
        case UART_TX_DONE:
            memset ( tx_buf, 0, TX_BUFF_SIZE );
            k_msleep ( 2 );
            gpio_pin_set_dt ( &rs485de, 0 );
            k_condvar_signal ( &txDoneVar );
            break;
        case UART_RX_RDY:
            add_rx_bytes ( evt->data.rx.buf,
                           evt->data.rx.offset,
                           evt->data.rx.len );
            break;
        case UART_RX_DISABLED:
            LOG_WRN ( "UART_RX_DISABLED" );
            enable_rx();
            break;
        case UART_TX_ABORTED:
            LOG_WRN ( "UART_TX_ABORTED" );
            gpio_pin_set_dt ( &rs485de, 0 );
            memset ( tx_buf, 0, TX_BUFF_SIZE );
            break;
        case UART_RX_BUF_REQUEST:
            switch_rx_buf();
            break;
        case UART_RX_BUF_RELEASED:
            release_rx_buf();
            break;
        case UART_RX_STOPPED:
            LOG_WRN ( "UART_RX_STOPPED" );
            break;
    }
}

void main ( void )
{
    LOG_INF ( "Starting application, board: %s", CONFIG_BOARD );
    LOG_INF ( "Software: %s:%s", GIT_BRANCH, GIT_COMMIT_HASH );

    LOG_INF ( "Registering callbacks..." );
    setSendMsgCb ( send_cmd );
    ftmsSetTargetsCb ( updateBikeTgts );
    // fecSetTargetsCb ( updateBikeTgts );

    LOG_INF ( "Configuring GPIO..." );
    int ret = 0;
    if ( !device_is_ready ( led.port ) ) {
        ret++;
    }
    if ( !device_is_ready ( rs485de.port ) ) {
        ret++;
    }
    if ( !device_is_ready ( lcdRst.port ) ) {
        ret++;
    }
    if ( !device_is_ready ( cptRst.port ) ) {
        ret++;
    }
    if ( !device_is_ready ( addInc.port ) ) {
        ret++;
    }
    if ( !device_is_ready ( subInc.port ) ) {
        ret++;
    }
    if ( !device_is_ready ( addRes.port ) ) {
        ret++;
    }
    if ( !device_is_ready ( subRes.port ) ) {
        ret++;
    }
    if ( ret ) {
        LOG_ERR ( "A port was not ready!" );
        return;
    }
    if ( gpio_pin_configure_dt ( &led, GPIO_OUTPUT_ACTIVE ) < 0 ) {
        ret++;
    }
    if ( gpio_pin_configure_dt ( &rs485de, GPIO_OUTPUT_INACTIVE ) < 0 ) {
        ret++;
    }
    if ( gpio_pin_configure_dt ( &lcdRst, GPIO_OUTPUT_INACTIVE ) < 0 ) {
        ret++;
    }
    if ( gpio_pin_configure_dt ( &cptRst, GPIO_OUTPUT_INACTIVE ) < 0 ) {
        ret++;
    }
    if ( gpio_pin_configure_dt ( &addInc, GPIO_INPUT ) < 0 ) {
        ret++;
    }
    if ( gpio_pin_configure_dt ( &subInc, GPIO_INPUT ) < 0 ) {
        ret++;
    }
    if ( gpio_pin_configure_dt ( &addRes, GPIO_INPUT ) < 0 ) {
        ret++;
    }
    if ( gpio_pin_configure_dt ( &subRes, GPIO_INPUT ) < 0 ) {
        ret++;
    }
    if ( ret ) {
        LOG_ERR ( "Configuration failed of %d devices!", ret );
        return;
    }
    if ( gpio_pin_interrupt_configure_dt ( &addInc, GPIO_INT_EDGE_TO_ACTIVE )
         != 0 ) {
        ret++;
    }
    if ( gpio_pin_interrupt_configure_dt ( &subInc, GPIO_INT_EDGE_TO_ACTIVE )
         != 0 ) {
        ret++;
    }
    if ( gpio_pin_interrupt_configure_dt ( &addRes, GPIO_INT_EDGE_TO_ACTIVE )
         != 0 ) {
        ret++;
    }
    if ( gpio_pin_interrupt_configure_dt ( &subRes, GPIO_INT_EDGE_TO_ACTIVE )
         != 0 ) {
        ret++;
    }
    if ( ret ) {
        LOG_ERR ( "Setting of pin interrupt failed on %d devices!", ret );
        return;
    }
    gpio_init_callback ( &addIncCbData, incPressed, BIT ( addInc.pin ) );
    gpio_init_callback ( &subIncCbData, incPressed, BIT ( subInc.pin ) );
    gpio_init_callback ( &addResCbData, resPressed, BIT ( addRes.pin ) );
    gpio_init_callback ( &subResCbData, resPressed, BIT ( subRes.pin ) );
    if ( gpio_add_callback ( addInc.port, &addIncCbData ) != 0 ) {
        ret++;
    }
    if ( gpio_add_callback ( subInc.port, &subIncCbData ) != 0 ) {
        ret++;
    }
    if ( gpio_add_callback ( addRes.port, &addResCbData ) != 0 ) {
        ret++;
    }
    if ( gpio_add_callback ( subRes.port, &subResCbData ) != 0 ) {
        ret++;
    }
    if ( ret ) {
        LOG_ERR ( "Setting of input callbacks failed on %d devices!", ret );
        return;
    }

    LOG_INF ( "Starting Uart1..." );
    uart = DEVICE_DT_GET ( DT_NODELABEL ( uart1 ) );
    if ( !device_is_ready ( uart ) ) {
        LOG_ERR ( "Uart1 not ready!" );
        return;
    }
    LOG_INF ( "Checking Uart1 ready..." );
    uint32_t start_ms = k_uptime_get_32();
    do {
        ret = uart_err_check ( uart );
        if ( ret ) {
            uint32_t end_ms = k_uptime_get_32();
            if ( end_ms - start_ms > 2000 ) {
                LOG_ERR ( "UART1 check failed: %d", ret );
                return;
            }
            k_msleep ( 10 );
        }
    } while ( ret );
    const struct uart_config uart_cfg
        = { .baudrate = 38400,
            .parity = UART_CFG_PARITY_NONE,
            .stop_bits = UART_CFG_STOP_BITS_1,
            .data_bits = UART_CFG_DATA_BITS_8,
            .flow_ctrl = UART_CFG_FLOW_CTRL_NONE };
    LOG_INF ( "Configuring Uart1..." );
    ret = uart_configure ( uart, &uart_cfg );
    if ( ret ) {
        LOG_ERR ( "Uart1 configure failure: %d", ret );
        return;
    }
    ret = uart_callback_set ( uart, uart_cb, NULL );
    if ( ret ) {
        LOG_ERR ( "Uart1 callback set failure: %d", ret );
        return;
    }
    ret = enable_rx();
    if ( ret ) {
        LOG_ERR ( "Uart1 rx buffer enable failure: %d", ret );
        return;
    }

    LOG_INF ( "Initializing bluetooth..." );
    //smp_bt_register();
    ret = bt_enable ( NULL );
    if ( ret ) {
        LOG_ERR ( "Bluetooth init failed (err %d)", ret );
        return;
    }
    LOG_INF ( "Starting advertising..." );
    adv_start();

    LOG_INF ( "Starting counter..." );
    if ( !device_is_ready ( rtc2_dev ) ) {
        LOG_ERR ( "Counter is not ready!" );
        return;
    }
    if ( counter_start ( rtc2_dev ) ) {
        LOG_ERR ( "Counter failed to start!" );
        return;
    }
    alarmCfg.flags = 0;
    alarmCfg.callback = counter_interrupt_cb;
    alarmCfg.user_data = &alarmCfg;

    // Configure nodes
    LOG_INF ( "Configuring bike nodes..." );
    initBike();

    // Start display last so user knows its ready
    LOG_INF ( "Starting Display..." );
    if ( initDisplay() ) {
        LOG_ERR ( "Display initialization failed!" );
        return;
    }

    LOG_INF ( "Startup complete!  Entering loop..." );
    uint32_t exec_ms;
    bike_data_t bikeData;
    while ( 1 ) {
        // Set start time
        start_ms = k_uptime_get_32();

        // Update bike
        gpio_pin_toggle_dt ( &led );
        updateBike();
        bikeData = getBikeData();
#if defined( CONFIG_BOARD_NRF52840DK_NRF52840 ) \
    || defined( CONFIG_BOARD_NRF52840DONGLE_NRF52840 ) \
    || defined ( CONFIG_BOARD_NRF5340DK_NRF5340_CPUAPP_NS )
        bikeData.act_rpm = ( sys_rand32_get() % 21 ) + 80;
        bikeData.watts = ( sys_rand32_get() % 101 ) + 200;
#endif

        // Update bluetooth services
        bt_cscs_bike_notify ( bikeData );
        bt_cps_notify ( bikeData );
        bt_ftms_bike_notify ( bikeData );
        // bt_fec_update ( bikeData );
        bt_ftms_status_notify();

        // Update display
        updateDisplay ( bikeData );

        // Sleep to hit cycle target
        exec_ms = k_uptime_get_32() - start_ms;
        if ( exec_ms < TGT_CYCLE_MS ) {
            k_msleep ( TGT_CYCLE_MS - exec_ms );
        }
    }
}
