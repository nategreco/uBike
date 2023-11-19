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

#include "display.h"

#include <lvgl.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/kscan.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "common.h"

#define TOUCH_CONTROLLER_NODE DT_ALIAS ( kscan0 )
#define BL_PWM_NODE DT_ALIAS ( blpwm )
#define PWM_PERIOD PWM_MSEC ( 1U )  // 1 kHz
#define DIM_MS 10000U               // 10s
#define ASLEEP_MS 60000U            // 60s
#define SEM_TIMEOUT K_MSEC ( 50 )

#define STACKSIZE 2048
#define PRIORITY 15

LOG_MODULE_REGISTER ( display );

K_SEM_DEFINE ( data_sem, 1, 1 );  // Default to taken
static const struct device *display_dev
    = DEVICE_DT_GET ( DT_CHOSEN ( zephyr_display ) );
static const struct pwm_dt_spec blPwm = PWM_DT_SPEC_GET ( BL_PWM_NODE );
static const struct device *kscan_dev = DEVICE_DT_GET ( TOUCH_CONTROLLER_NODE );
static bike_data_t bikeData = {};

static lv_obj_t *rpm_label;
static lv_obj_t *pwr_label;
static lv_obj_t *inc_label;
static lv_obj_t *res_label;
static lv_obj_t *rpm_desc_label;
static lv_obj_t *pwr_desc_label;
static lv_obj_t *inc_desc_label;
static lv_obj_t *res_desc_label;
static lv_obj_t *swLabel;
static lv_obj_t *btnLabel;
static lv_obj_t *btn;
static lv_style_t style;
static lv_style_t descStyle;
static lv_style_t btnStyle;
static stopwatch_data_t swData;
static char rpmString [4];  // 999
static char pwrString [5];  // 9999
static char incString [7];  // -10.0%
static char resString [3];  // 99
static char swString [9];   // 00:00:00

void updateBacklight ( bool wakeUp )
{
    static uint32_t lastActive_ms = 0;
    if ( wakeUp ) {
        lastActive_ms = k_uptime_get_32();
    }

    int64_t elapsed_ms = k_uptime_get_32() - lastActive_ms;
    if ( elapsed_ms < 0 ) {
        elapsed_ms += 1LL << 32;
    }

    uint8_t intensity;
    if ( elapsed_ms < DIM_MS ) {
        intensity = 100;
    } else if ( elapsed_ms < ASLEEP_MS ) {
        intensity = 10;
    } else {
        intensity = 5;
    }

    // Set backlight intensity
    pwm_set_dt ( &blPwm,
                 PWM_PERIOD,
                 ( PWM_PERIOD * ( 100 - intensity ) ) / 100 );
}

static void k_callback ( const struct device *dev,
                         uint32_t row,
                         uint32_t col,
                         bool pressed )
{
    ARG_UNUSED ( dev );
    if ( pressed ) {
        LOG_INF ( "row = %u col = %u\n", row, col );
        updateBacklight ( true );

        if ( col >= 410 ) {
            resetTime();
        }
    }
}

void resetTime()
{
    LOG_INF ( "Resetting timer..." );
    memset ( &swData, 0, sizeof ( stopwatch_data_t ) );
}

static void incrementStopwatch ( int64_t elapsed_ms )
{
    if ( elapsed_ms < 0 ) {
        elapsed_ms += 1LL << 32;
    }
    swData.ms += elapsed_ms;
    while ( swData.ms >= 1000 ) {
        swData.secs++;
        swData.ms -= 1000;
    }
    while ( swData.secs >= 60 ) {
        swData.mins++;
        swData.secs -= 60;
    }
    while ( swData.mins >= 60 ) {
        swData.hrs++;
        swData.mins -= 60;
    }
}

static void updateSwString()
{
    if ( swData.hrs < 10 ) {
        sprintf ( &swString [0], "0%u:", swData.hrs );
    } else {
        sprintf ( &swString [0], "%u:", swData.hrs );
    }
    if ( swData.mins < 10 ) {
        sprintf ( &swString [3], "0%u:", swData.mins );
    } else {
        sprintf ( &swString [3], "%u:", swData.mins );
    }
    if ( swData.secs < 10 ) {
        sprintf ( &swString [6], "0%u", swData.secs );
    } else {
        sprintf ( &swString [6], "%u", swData.secs );
    }
}

static void updateRpmString ( uint16_t act_rpm )
{
    sprintf ( &rpmString [0], "%u", act_rpm );
}

static void updatePwrString ( uint16_t watts )
{
    sprintf ( &pwrString [0], "%u", watts );
}

static void updateIncString ( uint16_t tgt_inc )
{
    int16_t per = ( ( int16_t )tgt_inc - 20 ) / 2;
    if ( tgt_inc < 20 ) {
        sprintf ( &incString [0], "-%d.%u%c", -per, tgt_inc % 2 ? 5 : 0, '%' );
    } else {
        sprintf ( &incString [0], "%d.%u%c", per, tgt_inc % 2 ? 5 : 0, '%' );
    }
}

static void updateResString ( uint16_t disp_res )
{
    sprintf ( &resString [0], "%u", disp_res );
}

static void updateLabels ( bike_data_t bikeData )
{
    updateRpmString ( bikeData.act_rpm );
    updatePwrString ( bikeData.watts );
    updateIncString ( bikeData.tgt_inc );
    updateResString ( bikeData.disp_res );
    updateSwString();

    lv_label_set_text_fmt ( rpm_label, "%s", rpmString );
    lv_label_set_text_fmt ( pwr_label, "%s", pwrString );
    lv_label_set_text_fmt ( inc_label, "%s", incString );
    lv_label_set_text_fmt ( res_label, "%s", resString );
    lv_label_set_text_fmt ( swLabel, "%s", swString );
}

static void updateStopwatch ( bool running )
{
    const uint32_t now_ms = k_uptime_get_32();
    if ( swData.running ) {
        incrementStopwatch ( now_ms - swData.last_ms );
    }
    swData.running = running;
    swData.last_ms = now_ms;
}

static void screenCb ( lv_event_t *e )
{
    updateBacklight ( true );
}

static void buttonCb ( lv_event_t *e )
{
    LOG_INF ( "Timer reset button depressed!" );
    resetTime();
}

static void drawButton()
{
    btn = lv_btn_create ( lv_scr_act() );
    //lv_obj_add_event_cb ( btn, buttonCb, LV_EVENT_PRESSED, NULL );
    lv_obj_align ( btn, LV_ALIGN_TOP_MID, 0, 410 );
    lv_obj_set_height ( btn, 60 );
    lv_obj_set_width ( btn, 300 );

    btnLabel = lv_label_create ( btn );
    lv_style_init ( &btnStyle );
    lv_style_set_text_font ( &btnStyle, &lv_font_montserrat_24 );
    lv_obj_add_style ( btnLabel, &btnStyle, 0 );
    lv_label_set_text ( btnLabel, "Reset" );
    lv_obj_center ( btnLabel );
}

/*static void sliderCb ( lv_event_t *e )
{
    lv_obj_t *slider = lv_event_get_target ( e );
    LOG_INF ( "Slider value: %d", lv_slider_get_value ( slider ) );
}

static void drawSlider()
{
    lv_obj_t * slider = lv_slider_create(lv_scr_act());
    lv_obj_align ( slider, LV_ALIGN_TOP_MID, 0, 440 );
    lv_obj_add_event_cb(slider, sliderCb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_height ( slider, 30 );
    lv_obj_set_width ( slider, 300 );
}*/

static void drawLines()
{
    static lv_point_t linePts1 [] = { { 0, 40 }, { 319, 40 } };
    static lv_point_t linePts2 [] = { { 0, 160 }, { 319, 160 } };
    static lv_point_t linePts3 [] = { { 0, 320 }, { 319, 320 } };

    static lv_style_t style;
    lv_style_init ( &style );
    lv_style_set_line_width ( &style, 4 );

    lv_obj_t *line1, *line2, *line3;
    line1 = lv_line_create ( lv_scr_act() );
    line2 = lv_line_create ( lv_scr_act() );
    line3 = lv_line_create ( lv_scr_act() );
    lv_line_set_points ( line1, linePts1, 2 );
    lv_line_set_points ( line2, linePts2, 2 );
    lv_line_set_points ( line3, linePts3, 2 );
    lv_obj_add_style ( line1, &style, 0 );
    lv_obj_add_style ( line2, &style, 0 );
    lv_obj_add_style ( line3, &style, 0 );
}

static void drawLabels()
{
    rpm_desc_label = lv_label_create ( lv_scr_act() );
    pwr_desc_label = lv_label_create ( lv_scr_act() );
    inc_desc_label = lv_label_create ( lv_scr_act() );
    res_desc_label = lv_label_create ( lv_scr_act() );

    lv_style_init ( &descStyle );
    lv_style_set_text_font ( &descStyle, &lv_font_montserrat_24 );
    lv_obj_align ( rpm_desc_label, LV_ALIGN_TOP_MID, 80, 120 );
    lv_obj_add_style ( rpm_desc_label, &descStyle, 0 );
    lv_obj_align ( pwr_desc_label, LV_ALIGN_TOP_MID, -80, 120 );
    lv_obj_add_style ( pwr_desc_label, &descStyle, 0 );
    lv_obj_align ( inc_desc_label, LV_ALIGN_TOP_MID, -80, 260 );
    lv_obj_add_style ( inc_desc_label, &descStyle, 0 );
    lv_obj_align ( res_desc_label, LV_ALIGN_TOP_MID, 80, 260 );
    lv_obj_add_style ( res_desc_label, &descStyle, 0 );

    lv_label_set_text ( rpm_desc_label, "Rpm" );
    lv_label_set_text ( pwr_desc_label, "Watts" );
    lv_label_set_text ( inc_desc_label, "Incline" );
    lv_label_set_text ( res_desc_label, "Resistance" );

    rpm_label = lv_label_create ( lv_scr_act() );
    pwr_label = lv_label_create ( lv_scr_act() );
    inc_label = lv_label_create ( lv_scr_act() );
    res_label = lv_label_create ( lv_scr_act() );
    swLabel = lv_label_create ( lv_scr_act() );

    lv_style_init ( &style );
    lv_style_set_text_font ( &style, &lv_font_montserrat_48 );
    lv_obj_align ( rpm_label, LV_ALIGN_TOP_MID, 80, 60 );
    lv_obj_add_style ( rpm_label, &style, 0 );
    lv_obj_align ( pwr_label, LV_ALIGN_TOP_MID, -80, 60 );
    lv_obj_add_style ( pwr_label, &style, 0 );
    lv_obj_align ( inc_label, LV_ALIGN_TOP_MID, -80, 200 );
    lv_obj_add_style ( inc_label, &style, 0 );
    lv_obj_align ( res_label, LV_ALIGN_TOP_MID, 80, 200 );
    lv_obj_add_style ( res_label, &style, 0 );
    lv_obj_align ( swLabel, LV_ALIGN_TOP_MID, 0, 340 );
    lv_obj_add_style ( swLabel, &style, 0 );
}

int initDisplay()
{
    static bool initialized = false;
    if ( initialized ) {
        LOG_WRN ( "Display already initialized!" );
        return 0;
    }

    if ( !device_is_ready ( display_dev ) ) {
        LOG_ERR ( "Display device not ready!" );
        return -1;
    }
    if ( !device_is_ready ( blPwm.dev ) ) {
        LOG_ERR ( "Backlight PWM device isn't ready!" );
        return -2;
    }
    if ( pwm_set_dt ( &blPwm, PWM_PERIOD, PWM_PERIOD ) ) {
        LOG_ERR ( "Failed to set backlight period!" );
        return -3;
    };
    if ( !device_is_ready ( kscan_dev ) ) {
        LOG_ERR ( "kscan device %s not ready", kscan_dev->name );
        return -4;
    }

    updateBacklight ( true );
    
    kscan_config ( kscan_dev, k_callback );
    kscan_enable_callback ( kscan_dev );

    drawLines();
    drawLabels();
    drawButton();
    // drawSlider();

    // Semaphore default to taken
    bikeData.tgt_inc = 20;
    resetTime();
    updateLabels ( bikeData );

    lv_obj_clear_flag ( lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE );
    //lv_obj_add_event_cb ( lv_scr_act(), screenCb, LV_EVENT_PRESSED, NULL );

    lv_task_handler();
    display_blanking_off ( display_dev );

    k_sem_give ( &data_sem );
    initialized = true;
    return 0;
}

static uint32_t reDrawDisplay()
{
    if ( k_sem_take ( &data_sem, SEM_TIMEOUT ) ) {
        LOG_WRN ( "reDrawDisplay() - Failed to get display data semaphore!" );
        return 500;
    }

    bool active = bikeData.act_rpm > 0;
    updateBacklight ( active );
    updateStopwatch ( active );
    updateLabels ( bikeData );

    uint32_t ret = lv_task_handler();

    k_sem_give ( &data_sem );
    return ret;
}

int updateDisplay ( bike_data_t data )
{
    //LOG_INF ("Updating display..." );
    if ( k_sem_take ( &data_sem, SEM_TIMEOUT ) ) {
        LOG_WRN ( "updateDisplay() - Failed to get display data semaphore!" );
        return -1;
    }

    bikeData = data;

    k_sem_give ( &data_sem );
    reDrawDisplay();
    //LOG_INF ("Display updated!" );
    return 0;
}

/*static void displayThread ( void )
{
    k_sem_take ( &data_sem, K_FOREVER );  // Released by init
    k_sem_give ( &data_sem );

    uint32_t sleep_ms;
    for ( ;; ) {
        sleep_ms = reDrawDisplay();
        k_msleep ( sleep_ms );
    }
}

K_THREAD_DEFINE ( display_thread_id,
                  STACKSIZE,
                  displayThread,
                  NULL,
                  NULL,
                  NULL,
                  PRIORITY,
                  0,
                  0 );*/
