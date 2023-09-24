#!/usr/bin/env python

# Universal Bike Controller
# Copyright (C) 2022-2023, Greco Engineering Solutions LLC
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

import serial

# Create serial port
ComPort = serial.Serial('COM7')
ComPort.baudrate = 38400
ComPort.bytesize = 7
ComPort.parity   = 'N'
ComPort.stopbits = 2

START =     b':'
TERM =      b'\r\n'
READ_RPM =  b'510300020000' # Trailed by 0 byte + 1 byte Checksum
REPLY_RPM = b'510302010200' # Trailed by 1 byte + 1 byte Checksum
SET_RES =   b'6106000500'   # Trailed by 1 byte + 1 byte Checksum
ACK_RES =   b'6106010500'   # Trailed by 1 byte + 1 byte Checksum
SET_INC =   b'4106000100'   # Trailed by 1 byte + 1 byte Checksum
ACK_INC =   b'4106010100'   # Trailed by 1 byte + 1 byte Checksum
READ_INC =  b'410300020000' # Trailed by 0 byte + 1 byte Checksum
REPLY_INC = b'410302010200' # Trailed by 1 byte + 1 byte Checksum

def getChecksum ( data ):
    sum = 0
    for i in range(0, len(data), 2):
        sum += int(data[i:i+2], 16)
    sum -= 1 << 8
    # sum = sum & 0xFF
    return str(hex(sum))[3:].upper()

if __name__ == '__main__':
    while True:
        # Get a packet
        data = ComPort.readline()

        # Verify start char
        if data[0:1] != START:
            print('Error, bad start character: ' + data[0:1])
            continue
        else:
            data = data[1:]
        # Verify term chars
        if data[-2:] != TERM:
            print('Error, bad terminating characters: ' + str(data[-2:]))
            continue
        else:
            data = data.rstrip() # Remove CR+LF
        # Verify and remove checksum
        checksum = bytes.decode(data[-2:])
        data = data[:-2]
        if checksum != getChecksum(data):
            print('Checksum error')
            continue

        # Decode
        if READ_RPM == data:
            None
        elif REPLY_RPM in data:
            rpm = int(data[-2:], 16)
            print('Actual rpm: ' + str(rpm))
        elif SET_RES in data:
            res = int(data[-2:], 16)
            print('Set resistance: ' + str(res))
        elif ACK_RES in data:
            res = int(data[-2:], 16)
            print('Acknowledge resistance: ' + str(res))
        elif SET_INC in data:
            inc = int(data[-2:], 16) / 2 - 10 # Scale to degrees
            print('Set incline:' + str(inc))
        elif ACK_INC in data:
            inc = int(data[-2:], 16) / 2 - 10 # Scale to degrees
            print('Acknowledge incline:' + str(inc))
        elif READ_INC in data:
            None
        elif REPLY_INC in data:
            inc = int(data[-2:], 16) / 2 - 10 # Scale to degrees
            print('Actual incline: ' + str(inc))
        else:
            print('Unknown format: ' + str(data))

ComPort.close() 