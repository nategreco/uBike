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

#include "asciiModbus.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const char *asciiLookup [256]
    = { "00", "01", "02", "03", "04", "05", "06", "07", "08", "09", "0A", "0B",
        "0C", "0D", "0E", "0F", "10", "11", "12", "13", "14", "15", "16", "17",
        "18", "19", "1A", "1B", "1C", "1D", "1E", "1F", "20", "21", "22", "23",
        "24", "25", "26", "27", "28", "29", "2A", "2B", "2C", "2D", "2E", "2F",
        "30", "31", "32", "33", "34", "35", "36", "37", "38", "39", "3A", "3B",
        "3C", "3D", "3E", "3F", "40", "41", "42", "43", "44", "45", "46", "47",
        "48", "49", "4A", "4B", "4C", "4D", "4E", "4F", "50", "51", "52", "53",
        "54", "55", "56", "57", "58", "59", "5A", "5B", "5C", "5D", "5E", "5F",
        "60", "61", "62", "63", "64", "65", "66", "67", "68", "69", "6A", "6B",
        "6C", "6D", "6E", "6F", "70", "71", "72", "73", "74", "75", "76", "77",
        "78", "79", "7A", "7B", "7C", "7D", "7E", "7F", "80", "81", "82", "83",
        "84", "85", "86", "87", "88", "89", "8A", "8B", "8C", "8D", "8E", "8F",
        "90", "91", "92", "93", "94", "95", "96", "97", "98", "99", "9A", "9B",
        "9C", "9D", "9E", "9F", "A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7",
        "A8", "A9", "AA", "AB", "AC", "AD", "AE", "AF", "B0", "B1", "B2", "B3",
        "B4", "B5", "B6", "B7", "B8", "B9", "BA", "BB", "BC", "BD", "BE", "BF",
        "C0", "C1", "C2", "C3", "C4", "C5", "C6", "C7", "C8", "C9", "CA", "CB",
        "CC", "CD", "CE", "CF", "D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7",
        "D8", "D9", "DA", "DB", "DC", "DD", "DE", "DF", "E0", "E1", "E2", "E3",
        "E4", "E5", "E6", "E7", "E8", "E9", "EA", "EB", "EC", "ED", "EE", "EF",
        "F0", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "FA", "FB",
        "FC", "FD", "FE", "FF" };

void convert_7N2_to_8N1 ( uint8_t *buff, size_t len )
{
    for ( int i = 0; i < len; i++ ) {
        buff [i] = buff [i] | 0x80;
    }
}

void convert_8N1to_7N2 ( uint8_t *buff, size_t len )
{
    for ( int i = 0; i < len; i++ ) {
        buff [i] = buff [i] & 0x7F;
    }
}

uint8_t calc_checksum ( uint8_t *buff, size_t len )
{
    char ascii_byte [2] = { 0 };
    uint8_t sum = 0;
    for ( int i = 0; i < len; i = i + 2 ) {
        memcpy ( ascii_byte, buff + i, 2 );
        sum += strtol ( ascii_byte, NULL, 16 );
    }
    return -sum;
}

size_t create_msg ( char *buff, cmd_msg_data_t data )
{
    buff [0] = START;
    buff [1] = asciiLookup [data.nodeId][0];
    buff [2] = asciiLookup [data.nodeId][1];
    buff [3] = asciiLookup [data.funcCode][0];
    buff [4] = asciiLookup [data.funcCode][1];
    buff [5] = asciiLookup [data.dataAddress & 0xFF00][0];
    buff [6] = asciiLookup [data.dataAddress & 0xFF00][1];
    buff [7] = asciiLookup [data.dataAddress & 0xFF][0];
    buff [8] = asciiLookup [data.dataAddress & 0xFF][1];
    buff [9] = asciiLookup [data.value & 0xFF00][0];
    buff [10] = asciiLookup [data.value & 0xFF00][1];
    buff [11] = asciiLookup [data.value & 0xFF][0];
    buff [12] = asciiLookup [data.value & 0xFF][1];
    uint8_t checksum = calc_checksum ( buff + 1, 12 );
    buff [13] = asciiLookup [checksum][0];
    buff [14] = asciiLookup [checksum][1];
    buff [15] = TERM [0];
    buff [16] = TERM [1];
    buff [17] = '\0';
    convert_7N2_to_8N1 ( buff, 17 );
    return 17;
}

uint8_t ascii_to_int_2 ( uint8_t *buff )
{
    char ascii_byte [2] = { 0 };
    memcpy ( ascii_byte, buff, 2 );
    return strtol ( ascii_byte, NULL, 16 );
}

uint16_t ascii_to_int_4 ( uint8_t *buff )
{
    char ascii_byte [4] = { 0 };
    memcpy ( ascii_byte, buff, 4 );
    return strtol ( ascii_byte, NULL, 16 );
}