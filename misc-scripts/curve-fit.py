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

def func(X, a, b, c, d, e, f, g, h, i, j, k, l, m):
    rpm, res = X
    torque = (a + b*rpm + c*rpm**2 + d*rpm**3 + e*rpm**4 + f*rpm**5) * \
             (g + h*res + i*res**2 + j*res**3 + k*res**4 + l*res**5 ) + m
    return torque*rpm

fileName = sys.argv[1]
data = np.loadtxt(fileName, delimiter=',',dtype=str,encoding="utf-8-sig")

rpm = []
res = []
pwr = []
for index, elem in np.ndenumerate(data):
    if index[0] == 0:
        continue
    if index[1] == 0:
        # RPM
        rpm.append(int(elem))
    elif index[1] == 1:
        # Resistance
        res.append(int(elem))
    elif index[1] == 2:
        # Watts
        pwr.append(int(elem))

fit = curve_fit(func, (rpm, res), pwr, None, maxfev=50000)
print(fit[0])

sum = 0.0
for (a, b, c) in zip(rpm, res, pwr):
    res = func((a,b), fit[0][0], fit[0][1], fit[0][2],
                      fit[0][3], fit[0][4], fit[0][5],
                      fit[0][6], fit[0][7], fit[0][8],
                      fit[0][9], fit[0][10], fit[0][11], fit[0][12])
    diff = c - res
    print(str(c) + "\t" + str(int(res)) + "\t" + str(int(diff)))
    sum += diff**2
print("Final root sum-of-squares: " + str(sum**0.5))