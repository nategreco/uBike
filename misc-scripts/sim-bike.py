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
import sys

# Create serial port
ComPort = serial.Serial('COM7')
ComPort.baudrate = 38400
ComPort.bytesize = 7
ComPort.parity   = 'N'
ComPort.stopbits = 2

START =     b':'
TERM =      b'\r\n'
READ_RPM =  b'510300020000'
REPLY_RPM = b'5103020102'
SET_RES =   b'61060005'
ACK_RES =   b'61060105'
SET_INC =   b'41060001'
ACK_INC =   b'41060101'
READ_INC =  b'410300020000'
REPLY_INC = b'4103020102'

CFG_CMD_1 =  b'61060007000F83'
CFG_CMD_2 =  b'6106000800BED3'
CFG_CMD_3 =  b'410600060000B3'
CFG_CMD_4 =  b'41060007003C76'
CFG_CMD_5 =  b'4106000900149C'
CFG_CMD_6 =  b'41060008003C75'

def checkChecksum(data):
    sum = 0
    for i in range(0, len(data), 2):
        sum += int(data[i:i+2], 16)
    return 0

def intToAsciiByteString(i, len):
    res = i.to_bytes(len, byteorder='big')
    return res.hex().encode(encoding='UTF-8',errors='strict').upper()

def getChecksum(data):
    sum = 0
    for i in range(0, len(data), 2):
        sum += int(data[i:i+2], 16)
    # TODO - This is flakey
    sum -= 1 << 8
    # sum = sum & 0xFF
    res = str(hex(sum))[3:].upper()
    return res.encode(encoding='UTF-8',errors='strict')

def sendMessage(data):
    msg = START + data + getChecksum(data) + TERM
    print("Sending msg:   " + str(msg))
    ComPort.write(msg)

def send_rpm_reply(rpm):
    resp = REPLY_RPM + intToAsciiByteString(rpm,2)
    sendMessage(resp)

def send_inc_reply(incline):
    resp = REPLY_INC + intToAsciiByteString(incline,2)
    sendMessage(resp)

def dummy_resp(data):
    resp = data[:12]
    resp = resp[:5] + b'\x31' + resp[6:]
    sendMessage(resp)
    return int(data[8:12], 16)


def adj_val(act, tgt):
    if tgt > act:
        return act + 1
    elif tgt < act:
        return act - 1
    else:
        return tgt

if __name__ == '__main__':
    rpm = int(sys.argv[1])
    act_incline = 10 # 0 degrees
    tgt_incline = act_incline
    while True:
        # Get a packet
        data = ComPort.readline()
        try:
            start = data.rindex(b':')
            if start != 0:
                print("Bad start index: " + str(pos) + "!")
                data = data[start:]
        except:
            print("Couldn't find start character!")
            continue
        #print("Received msg:  " + str(data))

        # Verify start char
        if data[0:1] != START:
            print('Error, bad start character: ' + str(data[0:1]))
            continue
        else:
            data = data[1:]
        # Verify term chars
        if data[-2:] != TERM:
            print('Error, bad terminating characters: ' + str(data[-2:]))
            continue
        else:
            data = data.rstrip() # Remove CR+LF
        # Verify checksum
        if checkChecksum(data):
            print('Checksum error')
            continue

        # Fake replies
        if READ_RPM in data:
            send_rpm_reply(rpm)
        elif SET_RES in data:
            res = dummy_resp(data)
            print("Setting new resistance: " + str(res))
        elif SET_INC in data:
            tgt_incline = dummy_resp(data)
            print("Setting new incline: " + str(tgt_incline))
        elif READ_INC in data:
            act_incline = adj_val(act_incline, tgt_incline)
            send_inc_reply(act_incline)
            print("Sending act incline: " + str(act_incline))
        elif CFG_CMD_1 in data:
            print("Configure cmd 1 received!")
            dummy_resp(data)
        elif CFG_CMD_2 in data:
            print("Configure cmd 2 received!")
            # TODO - Bug, checksum is wrong for this reply
            reply = b':6106010800BED2\r\n'
            ComPort.write(reply)
        elif CFG_CMD_3 in data:
            print("Configure cmd 3 received!")
            dummy_resp(data)
        elif CFG_CMD_4 in data:
            print("Configure cmd 4 received!")
            dummy_resp(data)
        elif CFG_CMD_5 in data:
            print("Configure cmd 5 received!")
            dummy_resp(data)
        elif CFG_CMD_6 in data:
            print("Configure cmd 6 received!")
            dummy_resp(data)
        else:
            print('Unknown format: ' + str(data))

ComPort.close()