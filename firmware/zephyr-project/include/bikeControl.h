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

#ifndef BIKE_CONTROL_H
#define BIKE_CONTROL_H

#include <zephyr/types.h>

#include "common.h"

#define INC_BUTTON_DLY_MS 250
#define RES_BUTTON_DLY_MS 750

#define RPM_NODE 0x51
#define INC_NODE 0x41
#define RES_NODE 0x61

#define CFG_DELAY_MS 3000

#define INIT_INC 0x0014
#define INIT_RES 0x003A

typedef enum
{
    DECREASE,
    NOTHING,
    INCREASE
} buttonStatus_t;

// Defined in main.c
typedef int ( *send_msg_callback_t ) ( const cmd_msg_data_t );

// Prototypes
void setSendMsgCb ( send_msg_callback_t func ); 
buttonStatus_t evaluateButton ( int up, int down );
void adjustIncline ( buttonStatus_t adj );
void adjustResistance ( buttonStatus_t adj );
void updateBikeTgts ( const bike_tgts_t tgts );  // set_targets_callback_t
void initBike();
int new_msg ( uint8_t *buff, size_t len );
void updateBike();
bike_data_t getBikeData();

#endif  // BIKE_CONTROL_H