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

import sys
import math
import numpy as np
import itertools 
from scipy.optimize import curve_fit

def func(X, a, b):
    return a*X**2 + b*X

inputRes = sys.argv[1]
inputName = "InputValuesRes" + inputRes + ".csv"
data = np.loadtxt(inputName, delimiter=',',dtype=str,encoding="utf-8-sig")
inputRes = int(inputRes)

rpm = []
pwr = []
for index, elem in np.ndenumerate(data):
    if index[0] == 0:
        continue
    if index[1] == 0:
        # RPM
        rpm.append(int(elem))
    elif index[1] == 1:
        # Watts
        pwr.append(int(elem))

fit = curve_fit(func, rpm, pwr, None, maxfev=50000)
#print(fit[0])
for testRpm in range(0, 205, 5):
    testWatts = func(testRpm, fit[0][0], fit[0][1])
    print(str(testRpm) + "," + str(inputRes) + "," + str(int(testWatts)))