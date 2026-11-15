# Universal Bike Controller
This repository is a fork of [nategreco/uBike](https://github.com/nategreco/uBike), an open-source drop-in replacement for the NordicTrack S-series studio cycle consoles.

The changes in this fork include an **LLM-generated, physics-based power calculation** for estimating watts. The motivation was based on an observation that resistance levels **2-8 consistently report only 1 W**, raising concerns that other resistance levels may misestimate power output.

## Calibrating Any Bike: Coast-Down as Ground Truth
The power model in this fork is more than a set of constants tuned to one machine. It is a **repeatable procedure** any owner can run to calibrate their *own* bike — **with or without a power meter**. The values published further down are a reasonable default; the real contribution is the method.

### Why it works without a power meter
A coast-down measures the flywheel's braking torque directly from physics. Spin the flywheel up, stop pedaling, and record the RPM decay. Because the flywheel's moment of inertia is known from its mass and radius, the deceleration *is* the braking torque at each speed:

```
I · (dω/dt) = −T_brake(ω, level)
```

Power at a given cadence then follows as `T_brake · ω`. The only instrument required is the tachometer the firmware already provides — no external power meter.

The reason this transfers to real riding is subtle but important: a magnetic brake applies torque as a function of flywheel **speed**, not of whether a rider is pedaling. The torque-vs-speed curve captured while coasting is therefore the same curve that resists you under load. **Coast-down ground truth is loaded ground truth.**

### Two tiers of accuracy
- **Coast-down only (no power meter):** a solid, physics-based estimate that corrects gross errors — for example the stock firmware reporting ~1 W across several resistance levels. Good enough for consistent, trainable numbers.
- **Power-meter validated:** using an independent power meter (e.g. Favero Assioma pedals) to validate and trim the coast-down fit yields reference-grade accuracy. In this fork's testing, that validation revealed the stock firmware reading **1.3–2.3× low**.

### What transfers between bikes, and what doesn't
Manufacturing tolerance, magnet strength, belt tension, and wear differ from unit to unit, so **the specific constants in this repo will not be exact for another bike.** What transfers is the **procedure**. Treat the shipped values as a starting default and recalibrate against your own coast-downs for best results.

### Procedure (summary)
1. Confirm your flywheel mass and radius (see *Measurements Taken* below). Inertia accuracy bounds everything downstream.
2. For each resistance level, spin up to a known RPM, stop pedaling, and log RPM vs time until it reaches zero. Firmware-logged timestamps are far more precise than filming the readout.
3. Fit braking torque vs speed per level (see the scripts in `misc-scripts/`), then convert to power at cadence.
4. *(Optional, for reference grade)* Record a parallel sweep with a power meter and trim the fit to match.

### Contributing
If you have a power meter, validation sweeps across levels and cadences are the most valuable thing you can contribute — they let the shipped defaults improve for owners who don't have one. Coast-down logs from other S-series models are welcome too.

## Measurements Taken
Flywheel Diameter: 455.65 mm
Flywheel Pulley Diameter: 58.3 mm
Crank Pulley Diameter: 310 mm
Crank -> Flywheel Ratio: ~5.317 : 1

## Assumptions Made
- The flywheel mass was provided at **14.515 kg (32 lb)** -- the advertised weight cited by NordicTrack and several independent reviews.
- Mechanical efficiency figure was estimated at 0.95.

## Coast-Down Experiment Data
Anchor values for the model were computed from several **60 rpm → 0 rpm coast down experiments**.

The results of these experiments are provided below. 

**Note:** This data was gathered by filming the uBike's console RPM readout. I may refine these measurements later as determining the precise zero-RPM timestamp from the footage introduced minor uncertainty.

<details> <summary><b>Resistance 1</b></summary>

| **time_s** | **rpm (run 1)** | **rpm (run 2)** |
| :--------: | :-------------: | :-------------: |
|     0.0    |        60       |        59       |
|     0.5    |        59       |                 |
|     1.0    |                 |        58       |
|     1.1    |        58       |                 |
|     1.5    |        56       |        55       |
|     2.0    |                 |        52       |
|     2.1    |        53       |                 |
|     2.5    |                 |        49       |
|     3.1    |        49       |                 |
|     3.5    |        46       |        45       |
|     4.5    |        42       |        41       |
|     5.5    |        38       |        37       |
|     6.5    |        34       |        33       |
|     7.5    |                 |        29       |
|     8.0    |        30       |                 |
|     9.5    |        25       |        25       |
|    11.5    |        0        |        0        |

<details> <summary><b>Resistance 4</b></summary>

| **time_s** | **rpm (run 1)** | **rpm (run 2)** |
| :--------: | :-------------: | :-------------: |
|     0.0    |        60       |        60       |
|     0.5    |        59       |        59       |
|     1.0    |        58       |                 |
|     1.5    |                 |        57       |
|     2.0    |        57       |        55       |
|     2.5    |        54       |        51       |
|     3.0    |        51       |                 |
|     3.5    |                 |        47       |
|     4.0    |        48       |        44       |
|     4.5    |        44       |                 |
|     5.0    |                 |        40       |
|     5.5    |        40       |                 |
|     6.0    |                 |        36       |
|     6.5    |        36       |                 |
|     7.0    |                 |        31       |
|     7.5    |        32       |                 |
|     8.5    |                 |        27       |
|     9.0    |        27       |                 |
|    10.5    |                 |        22       |
|    11.0    |        23       |                 |
|    12.5    |                 |        0        |
|    13.0    |        0        |                 |

<details> <summary><b>Resistance 10</b></summary>

| **time_s** | **rpm (run 1)** | **rpm (run 2)** |
| :--------: | :-------------: | :-------------: |
|     0.0    |        61       |        60       |
|     0.1    |                 |        59       |
|     0.3    |        60       |                 |
|     0.8    |        59       |                 |
|     1.0    |                 |        57       |
|     1.3    |        56       |                 |
|     2.0    |                 |        54       |
|     2.3    |        51       |                 |
|     2.5    |                 |        49       |
|     2.8    |        44       |                 |
|     3.5    |                 |        42       |
|     4.3    |        36       |                 |
|     5.0    |                 |        34       |
|     6.3    |        28       |                 |
|     7.0    |                 |        26       |
|     7.8    |        0        |                 |
|     9.0    |                 |        0        |

<details> <summary><b>Resistance 13</b></summary>

| **time_s** | **rpm (run 1)** | **rpm (run 2)** |
| :--------: | :-------------: | :-------------: |
|     0.0    |        60       |        59       |
|     0.2    |                 |        57       |
|     0.7    |                 |        54       |
|     1.0    |        58       |                 |
|     1.5    |        52       |                 |
|     1.7    |                 |        47       |
|     3.0    |        43       |                 |
|     3.2    |                 |        38       |
|     5.0    |        32       |                 |
|     5.2    |                 |        0        |
|     7.0    |        0        |                 |

<details> <summary><b>Resistance 16</b></summary>

| **time_s** | **rpm (run 1)** | **rpm (run 2)** |
| :--------: | :-------------: | :-------------: |
|     0.0    |        61       |        59       |
|     0.1    |                 |        58       |
|     0.5    |        59       |        56       |
|     1.5    |        52       |        50       |
|     3.0    |        41       |        40       |
|     5.0    |        0        |        0        |

<details> <summary><b>Resistance 18</b></summary>

| **time_s** | **rpm (run 1)** | **rpm (run 2)** |
| :--------: | :-------------: | :-------------: |
|     0.0    |        61       |        61       |
|     0.1    |                 |        59       |
|     0.9    |        59       |                 |
|     1.1    |                 |        51       |
|     1.9    |        52       |                 |
|     3.1    |                 |        0        |
|     3.4    |        0        |                 |

<details> <summary><b>Resistance 20</b></summary>

| **time_s** | **rpm (run 1)** | **rpm (run 2)** |
| :--------: | :-------------: | :-------------: |
|     0.0    |        60       |        60       |
|     0.5    |        58       |                 |
|     0.7    |                 |        56       |
|     1.5    |        48       |                 |
|     2.3    |                 |        46       |
|     3.5    |        0        |                 |
|     3.7    |                 |        0        |

<details> <summary><b>Resistance 22</b></summary>

| **time_s** | **rpm (run 1)** | **rpm (run 2)** |
| :--------: | :-------------: | :-------------: |
|     0.0    |        60       |        59       |
|     0.5    |                 |        55       |
|     0.6    |        56       |                 |
|     2.0    |                 |        0        |
|     2.6    |        0        |                 |
