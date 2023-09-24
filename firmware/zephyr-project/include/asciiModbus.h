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

#ifndef ASCII_MODBUS_H
#define ASCII_MODBUS_H

#include <zephyr/types.h>

#include "common.h"

// String identifiers
#define START ':'
#define TERM "\r\n"
#define START_CHAR_8 ( ':' | 0x80 )
#define TERM_CHAR_8 ( '\n' | 0x80 )

// Function codes
#define READ_COIL 0x01
#define READ_INPT 0x02
#define READ_MULTI_HOLD 0x03
#define READ_REGR 0x04
#define WRITE_COIL 0x05
#define WRITE_HOLD 0x06
#define WRITE_MULTI_COIL 0x0F
#define WRITE_MULTI_HOLD 0x10

void convert_7N2_to_8N1 ( uint8_t *buff, size_t len );
void convert_8N1to_7N2 ( uint8_t *buff, size_t len );
uint8_t calc_checksum ( uint8_t *buff, size_t len );
size_t create_msg ( char *buff, cmd_msg_data_t data );
uint8_t ascii_to_int_2 ( uint8_t *buff );
uint16_t ascii_to_int_4 ( uint8_t *buff );

#endif  // ASCII_MODBUS_H