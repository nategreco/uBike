# Misc Scripts
A few scripts that were used to reverse engineer the bike:
* serial-test.py
  * A script for sniffing the RS-485 comms
* sim-bike.py
  * A script that simulates the bike for building a table of wattage based on a given cadence and resistance
* create-data.py
  * A script for building additional wattage data beyond what was manually collected
* curve-fit.py
  * A script to curve-fit a polynomial to the wattage data

The wattage calculation relies on curve-fit data manually collected from the console when simulating an input cadence. Because of the spareness of the data, additional 'fake' data was produced for input to the curve fitting.
