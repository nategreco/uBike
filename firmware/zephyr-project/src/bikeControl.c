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

#include "bikeControl.h"

#include <math.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "asciiModbus.h"


LOG_MODULE_REGISTER ( bike );
static send_msg_callback_t sendMsgCbFunc = NULL;

// Global variables
static cmd_msg_data_t CFG_CMD_1 = { RES_NODE, WRITE_HOLD, 0x0007, 0x000F };
static cmd_msg_data_t CFG_CMD_2 = { RES_NODE, WRITE_HOLD, 0x0008, 0x00BE };
static cmd_msg_data_t CFG_CMD_3 = { INC_NODE, WRITE_HOLD, 0x0006, 0x0000 };
static cmd_msg_data_t CFG_CMD_4 = { INC_NODE, WRITE_HOLD, 0x0007, 0x003C };
static cmd_msg_data_t CFG_CMD_5 = { INC_NODE, WRITE_HOLD, 0x0009, 0x0014 };
static cmd_msg_data_t CFG_CMD_6 = { INC_NODE, WRITE_HOLD, 0x0008, 0x003C };
static cmd_msg_data_t RPM_REQ = { RPM_NODE, READ_MULTI_HOLD, 0x0002, 0x0000 };
static cmd_msg_data_t INC_REQ = { INC_NODE, READ_MULTI_HOLD, 0x0002, 0x0000 };
static cmd_msg_data_t SET_RES = { RES_NODE, WRITE_HOLD, 0x0005, INIT_RES };
static cmd_msg_data_t SET_INC = { INC_NODE, WRITE_HOLD, 0X0001, INIT_INC };
/*static cmd_msg_data_t ZERO_RPM
    = { INC_NODE, WRITE_HOLD, 0X0004, 0x0000 };*/  // TODO - Send on stop

// Control parameters
static uint16_t act_rpm = 0;
static uint16_t act_inc = INIT_INC;
static uint16_t disp_res = 1;
static bool firstRead = false;

void setSendMsgCb ( send_msg_callback_t func )
{
    sendMsgCbFunc = func;
}

void sendWithRetries ( cmd_msg_data_t cmd, uint16_t retries, int32_t delay_ms )
{
    int res;
    for ( int i = 0; i < retries + 1; i++ ) {
        res = sendMsgCbFunc ( cmd );
        if ( !res ) {
            return;
        } else {
            LOG_ERR ( "Failed to send on attempt %d out of %u.  Returned: %d",
                      i + 1,
                      retries + 1,
                      res );
        }
        k_msleep ( delay_ms );
    }
}

// Evalute user inputs
buttonStatus_t evaluateButton ( int up, int down )
{
    if ( up && !down ) {
        return INCREASE;
    } else if ( down && !up ) {
        return DECREASE;
    }
    return NOTHING;
}

static uint16_t calc_res()
{
    int16_t res = 15;

    // +1 display resistance is worth 5 counts
    res += 5 * ( disp_res - 1 );

    // 1% grade is worth 4 counts
    res += 4 * ( ( act_inc - 20 ) / 2 );

    // Clip
    if ( res < 15 ) {
        return 15;
    } else if ( res > 190 ) {
        return 190;
    }
    return res;
}

void adjustIncline ( buttonStatus_t adj )
{
    if ( ( adj == INCREASE ) && ( SET_INC.value < 60 ) ) {
        SET_INC.value++;
        LOG_INF ( "Increasing incline to: %d", SET_INC.value );
    } else if ( ( adj == DECREASE ) && ( SET_INC.value > 0 ) ) {
        SET_INC.value--;
        LOG_INF ( "Decreasing incline to: %d", SET_INC.value );
    }
}

void adjustResistance ( buttonStatus_t adj )
{
    if ( ( adj == INCREASE ) && ( disp_res < 22 ) ) {
        disp_res++;
        LOG_INF ( "Increasing resistance to: %d", disp_res );
    } else if ( ( adj == DECREASE ) && ( disp_res > 1 ) ) {
        disp_res--;
        LOG_INF ( "Decreasing resistance to: %d", disp_res );
    }
}

static void setIncline ( uint16_t tgt )
{
    LOG_INF ( "Setting incline to: %u", tgt );
    if ( tgt > 60 ) {
        SET_INC.value = 60;
    } else {
        SET_INC.value = tgt;
    }
}

static void setResistance ( uint16_t tgt )
{
    LOG_INF ( "Setting resistance to: %u", tgt );
    if ( tgt > 22 ) {
        SET_INC.value = 22;
    } else if ( tgt <= 1 ) {
        SET_INC.value = 1;
    } else {
        SET_INC.value = tgt;
    }
}

// Update bike targets
void updateBikeTgts ( const bike_tgts_t tgts )
{
    // Incline
    if ( tgts.incline != 0x7FFF ) {
        if ( tgts.incline >= 2000 ) {
            setIncline ( 60 );
        } else if ( tgts.incline <= -1000 ) {
            setIncline ( 0 );
        } else {
            uint16_t roundUp = ( tgts.incline + 1000 ) % 50 > 25 ? 1 : 0;
            setIncline ( ( tgts.incline + 1000 ) / 50 + roundUp );
        }
    }

    // Resistance
    if ( tgts.resistance != 0xFF ) {
        if ( tgts.resistance >= 200 ) {
            setResistance ( 22 );
        } else if ( tgts.resistance == 0 ) {
            setResistance ( 1 );
        } else {
            uint16_t roundUp = ( tgts.resistance * 21 ) % 200 > 25 ? 1 : 0;
            setResistance ( 1 + ( tgts.resistance * 21 ) / 200 + roundUp );
        }
    }
}

void initBike()
{
    if ( !sendMsgCbFunc ) {
        LOG_ERR ( "Send message callback not registered!" );
        return;
    }
    k_msleep ( CFG_DELAY_MS );
    sendWithRetries ( CFG_CMD_1, 50, 200 );
    sendWithRetries ( CFG_CMD_2, 5, 50 );
    sendWithRetries ( CFG_CMD_3, 5, 50 );
    sendWithRetries ( CFG_CMD_4, 5, 50 );
    sendWithRetries ( CFG_CMD_5, 5, 50 );
    sendWithRetries ( CFG_CMD_6, 5, 50 );
    sendWithRetries ( SET_RES, 5, 50 );
    sendWithRetries ( INC_REQ, 5, 50 );
}

int new_msg ( uint8_t *buff, size_t len )
{
    convert_8N1to_7N2 ( buff, len );
    // Remove start
    if ( buff [0] != ':' ) {
        return -1;
    }
    buff++;
    // Remove termination characters
    if ( ( buff [len - 3] == '\r' ) && ( buff [len - 2] == '\n' ) ) {
        return -2;
    }
    len -= 3;

    // Check Checksum
    if ( calc_checksum ( buff, len ) ) {
        return -3;
    }

    // Get params
    uint8_t nodeId = ascii_to_int_2 ( buff );
    uint8_t funcCode = ascii_to_int_2 ( buff + 2 );
    if ( funcCode == WRITE_HOLD ) {
        // Assume a succesful write
        return 0;
    } else if ( funcCode == READ_MULTI_HOLD ) {
        //  This is a reply, we need to extract data
        uint16_t value = ascii_to_int_4 ( buff + 10 );
        if ( nodeId == RPM_NODE ) {
            act_rpm = value;
            return 0;
        } else if ( nodeId == INC_NODE ) {
            if ( !firstRead ) {
                SET_INC.value = value;
                firstRead = true;
            }
            act_inc = value;
            return 0;
        } else {
            return -4;  // Unhandled node id
        }
    }

    return -5;  // Unhandled func code
}

static float power ( float base, int pwr )
{
    return pwr == 0 ? 1.0 : base * power ( base, pwr - 1 );
}

// Power curve fitting
#define A -8.06357866e+06
#define B -2.45457703e+05
#define C 1.49961556e+01
#define D -6.48778450e-02
#define E 1.09741619e-04
#define F 3.16050229e-09
#define G -1.00397536e-07
#define H 1.01525992e-08
#define I -2.94388976e-10
#define J 3.48657332e-12
#define K -1.97940519e-14
#define L 4.09271382e-17
#define M 5.62850051e-02

static uint16_t calc_watts()
{
    // If no motion return 0
    if ( act_rpm == 0 ) {
        return 0;
    }

    // Caculate wats in N*m
    float pwr = ( A + B * act_rpm + C * power ( act_rpm, 2 )
                  + D * power ( act_rpm, 3 ) + E * power ( act_rpm, 4 )
                  + F * power ( act_rpm, 5 ) )
                    * ( G + H * SET_RES.value + I * power ( SET_RES.value, 2 )
                        + J * power ( SET_RES.value, 3 )
                        + K * power ( SET_RES.value, 4 )
                        + L * power ( SET_RES.value, 5 ) )
                + M;

    // Calculate watts from torque
    pwr *= act_rpm;

    // If motion don't return less than one
    if ( pwr < 1.0 ) {
        return 1;
    }
    return pwr;
}

static void updateResistance()
{
    const uint16_t new_res = calc_res();
    if ( SET_RES.value != new_res ) {
        SET_RES.value = new_res;
        LOG_INF ( "Changing resistance magnitude to: %d", new_res );
        sendWithRetries ( SET_RES, 3, 50 );
    }
}

void updateBike()
{
    sendWithRetries ( RPM_REQ, 0, 0 );
    if ( firstRead && ( act_inc != SET_INC.value ) ) {
        sendWithRetries ( SET_INC, 1, 50 );
        sendWithRetries ( INC_REQ, 0, 50 );
    }
    updateResistance();
}

bike_data_t getBikeData()
{
    bike_data_t data;
    data.act_rpm = act_rpm;
    data.disp_res = disp_res;
    data.tgt_inc = SET_INC.value;
    data.watts = calc_watts();
    return data;
}