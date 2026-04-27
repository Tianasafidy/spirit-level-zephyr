# Spirit Level on Zephyr

This project is a Zephyr-based embedded application implementing a digital spirit level on an 8x8 RGB LED matrix.

The application uses the LSM6DSL accelerometer/gyroscope available on the target board to estimate the tilt of the board. The measured angle is then displayed on an RGB LED matrix driven by a DM163 LED controller.

The project was developed for the `disco_l475_iot1` board and demonstrates sensor acquisition, GPIO-based LED matrix control, custom Zephyr drivers, timers, threads and inter-thread communication.

## Project Overview

The goal of this project is to reproduce the behavior of a physical spirit level using an embedded board and an RGB LED matrix.

The board continuously measures its inclination using the IMU sensor. According to the tilt direction, a square moves on the LED matrix. When the board is close to the horizontal position, the square is displayed in green. When the board is tilted, the square is displayed in red.

## Main Features

- Real-time tilt measurement using the LSM6DSL IMU.
- Accelerometer and gyroscope acquisition through I2C.
- Complementary filtering to combine accelerometer and gyroscope information.
- 8x8 RGB LED matrix display.
- Custom Zephyr LED driver for the SITI DM163 LED controller.
- Row-by-row LED matrix refresh.
- Zephyr timer-based display update.
- Multi-threaded application structure.
- Thread-safe image update using mutexes and message queues.
- Visual level indication:
  - green square when the board is approximately level,
  - red square when the board is tilted.

## Hardware

The project targets the following hardware setup:

- ST B-L475E-IOT01A / `disco_l475_iot1` board.
- On-board LSM6DSL accelerometer/gyroscope.
- External 8x8 RGB LED matrix.
- SITI DM163 LED driver.
- GPIO lines for DM163 control signals and matrix row selection.

The hardware configuration is described in the board overlay file:

```text
dm163_example/boards/disco_l475_iot1.overlay
```

## Repository Structure

```text
.
├── README.md
└── dm163_example
    ├── boards
    │   └── disco_l475_iot1.overlay
    ├── dm163_module
    │   └── zephyr
    │       ├── dm163.c
    │       ├── dm163.h
    │       ├── Kconfig
    │       ├── CMakeLists.txt
    │       └── module.yml
    ├── dts
    │   └── bindings
    │       ├── rgb_matrix.yaml
    │       └── siti,dm163.yaml
    ├── include
    │   ├── accel.h
    │   └── spirit_level.h
    ├── src
    │   ├── accel.c
    │   ├── main.c
    │   └── spirit_level.c
    ├── CMakeLists.txt
    └── prj.conf
```

## Software Architecture

The application is divided into three main parts:

1. DM163 LED driver
2. IMU acquisition and tilt estimation
3. Spirit level display logic

## DM163 LED Driver

The DM163 driver is implemented as a custom Zephyr LED driver.

Main files:

```text
dm163_example/dm163_module/zephyr/dm163.c
dm163_example/dm163_module/zephyr/dm163.h
```

The driver is responsible for:

- configuring the GPIO pins used by the DM163,
- setting LED brightness,
- sending RGB channel data,
- flushing data to the LED driver,
- controlling the row selection of the LED matrix.

The DM163 communication is done through GPIO bit-banging.

## Accelerometer and Gyroscope Handling

The IMU management is implemented in:

```text
dm163_example/src/accel.c
```

This part of the application:

- initializes the LSM6DSL sensor,
- configures accelerometer and gyroscope acquisition,
- reads raw sensor values,
- converts the data into usable physical values,
- estimates the board tilt angle.

A complementary filter is used to combine accelerometer and gyroscope data. This helps to reduce noise while keeping a responsive angle estimation.

## Spirit Level Display Logic

The spirit level logic is implemented in:

```text
dm163_example/src/spirit_level.c
```

This module converts the measured tilt into a position on the 8x8 LED matrix.

The display behavior is simple:

- when the board is centered, a green square is displayed,
- when the board is tilted, a red square moves in the tilt direction.

The tilt values are clamped before being converted into matrix coordinates, so the displayed square always stays inside the matrix area.

## How It Works

1. The application initializes the DM163 LED controller.
2. The GPIO pins used for the RGB matrix rows are configured.
3. A Zephyr timer periodically triggers the display refresh.
4. A display thread scans the matrix row by row.
5. The LSM6DSL sensor is configured over I2C.
6. New accelerometer and gyroscope values are read.
7. The tilt angle is estimated using a complementary filter.
8. The spirit level logic converts the tilt into LED matrix coordinates.
9. The RGB matrix displays the current tilt position.

## Display Refresh

The LED matrix is refreshed row by row.

For each row:

1. The row is selected.
2. RGB data is sent to the DM163.
3. The data is latched.
4. The next row is displayed at the next refresh step.

This scanning process gives the impression that the whole matrix is continuously displayed.

## Requirements

To build and run this project, the following tools are required:

- Zephyr development environment.
- Zephyr SDK.
- `west` command-line tool.
- A supported board: `disco_l475_iot1`.
- Hardware wiring matching the device tree overlay.

## Build

From the repository root, build the application with:

```bash
west build -b disco_l475_iot1 dm163_example
```

If the build directory already exists and needs to be regenerated, use:

```bash
west build -b disco_l475_iot1 dm163_example -p always
```

## Flash

To flash the application to the board:

```bash
west flash
```

## Serial Output

The application prints debug information on the serial console.

Example output:

```text
Sensor reset successfully !!!!
Set up lsm6dsl_int1 at ...
X next: 3 ; Y next 4 ; X tilt 0 ; Y tilt 0
```

The output can be used to check that the sensor is correctly initialized and that tilt values are being updated.

## Main Files

| File | Description |
|---|---|
| `src/main.c` | Main application entry point, display thread and timer setup. |
| `src/accel.c` | LSM6DSL configuration, sensor reading and tilt estimation. |
| `src/spirit_level.c` | Conversion of tilt values into LED matrix display positions. |
| `dm163_module/zephyr/dm163.c` | Custom Zephyr LED driver for the DM163 controller. |
| `dm163_module/zephyr/dm163.h` | DM163 driver public interface. |
| `boards/disco_l475_iot1.overlay` | Device tree overlay defining GPIO connections. |
| `dts/bindings/siti,dm163.yaml` | Device tree binding for the DM163 controller. |
| `dts/bindings/rgb_matrix.yaml` | Device tree binding for the RGB matrix row GPIOs. |
| `prj.conf` | Zephyr project configuration. |

## Technologies Used

- C
- Zephyr RTOS
- Device Tree
- GPIO
- I2C
- Zephyr device drivers
- Zephyr threads
- Zephyr timers
- LSM6DSL IMU
- DM163 LED driver
- RGB LED matrix

## Notes

- The current implementation is configured for an 8x8 RGB matrix.
- The project uses a custom DM163 driver integrated as a Zephyr module.
- The IMU is used to estimate board orientation in real time.
- The display is refreshed periodically using a Zephyr timer.
- The project demonstrates how to combine sensor acquisition, embedded drivers and display control in a Zephyr-based application.

## Possible Improvements

Some possible improvements could include:

- adding calibration for the accelerometer and gyroscope,
- improving the filtering of tilt values,
- adding different colors depending on the tilt angle,
- making the display refresh rate configurable,
- adding support for other LED matrix sizes,
- cleaning debug messages for a production version.

## License

No license information is currently provided in the repository.
