# Spirit Level on Zephyr

This project is a Zephyr-based embedded application implementing a digital spirit level on an RGB LED matrix. It uses the on-board LSM6DSL accelerometer/gyroscope to estimate the board tilt and displays the result on an 8x8 RGB matrix driven by a DM163 LED controller.

The application was developed for the `disco_l475_iot1` board and demonstrates sensor acquisition, GPIO-based LED matrix driving, custom Zephyr device drivers, multi-threading and inter-thread communication.

## Features

- Real-time tilt measurement using the LSM6DSL IMU.
- Accelerometer and gyroscope data acquisition over I2C.
- Complementary filtering to combine accelerometer and gyroscope tilt information.
- 8x8 RGB LED matrix display.
- Custom Zephyr LED driver for the SITI DM163 LED controller.
- Row-by-row LED matrix refresh using a Zephyr timer and display thread.
- Thread-safe image update using mutexes and message queues.
- Visual level indication:
  - green square when the board is approximately level,
  - red square when the board is tilted.

## Hardware

The project targets the following hardware setup:

- ST B-L475E-IOT01A / `disco_l475_iot1` board.
- LSM6DSL accelerometer/gyroscope connected through I2C.
- External RGB LED matrix controlled by a SITI DM163 LED driver.
- GPIO lines used for DM163 control signals and matrix row selection.

The GPIO configuration is defined in:

```text
dm163_example/boards/disco_l475_iot1.overlay
Project Structure
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
Software Architecture

The application is split into three main parts.

1. DM163 LED Driver

The custom DM163 driver is implemented as a Zephyr LED driver in:

dm163_example/dm163_module/zephyr/dm163.c

It provides functions for setting LED brightness, writing RGB channel values, flushing channel data to the DM163 and controlling matrix row switching.

2. Accelerometer and Gyroscope Handling

The IMU management is implemented in:

dm163_example/src/accel.c

It configures the LSM6DSL sensor, reads acceleration and gyroscope data, converts raw values into physical units, and estimates the board tilt. A complementary filter is used to combine accelerometer-based tilt with gyroscope-based motion information.

3. Spirit Level Display Logic

The display logic is implemented in:

dm163_example/src/spirit_level.c

The tilt values are mapped to a position on the 8x8 LED matrix. A 2x2 square is displayed in green when the board is centered, and in red when the board is tilted.

How It Works
The application initializes the DM163 LED controller and the matrix row GPIOs.
A Zephyr timer periodically triggers the display refresh.
The display thread scans the matrix row by row and writes RGB channel data to the DM163.
The LSM6DSL IMU is configured over I2C.
New accelerometer/gyroscope data is read when the sensor interrupt is triggered.
A complementary filter estimates the board tilt.
The spirit level thread converts the tilt angle into an LED matrix position.
The RGB matrix displays a moving square showing the current tilt direction.
Requirements
Zephyr development environment installed.
Zephyr SDK installed.
west command-line tool configured.
Supported board: disco_l475_iot1.
External RGB matrix and DM163 wiring matching the device tree overlay.
Build

From the repository root, build the Zephyr application with:

west build -b disco_l475_iot1 dm163_example

If the build directory already exists and needs to be regenerated:

west build -b disco_l475_iot1 dm163_example -p always
Flash

To flash the application to the board:

west flash
Serial Output

The application prints debug information over the serial console, including tilt values and sensor initialization messages.

Example output:

Sensor reset successfully !!!!
Set up lsm6dsl_int1 at ...
X next: 3 ; Y next 4 ; X tilt 0 ; Y tilt 0
Main Files
File	Description
src/main.c	Application entry point, display thread and timer setup.
src/accel.c	LSM6DSL configuration, sensor reading and complementary filter.
src/spirit_level.c	Tilt-to-matrix mapping and spirit level display logic.
dm163_module/zephyr/dm163.c	Custom Zephyr LED driver for the DM163 controller.
boards/disco_l475_iot1.overlay	Hardware description for DM163 pins and RGB matrix rows.
dts/bindings/siti,dm163.yaml	Device tree binding for the DM163 controller.
dts/bindings/rgb_matrix.yaml	Device tree binding for the RGB matrix rows.
Notes
The current implementation is configured for an 8x8 RGB matrix.
The display refresh period is defined in main.c using a Zephyr timer.
The tilt range is clamped before being mapped to the matrix position.
The DM163 driver uses GPIO bit-banging to transmit channel and brightness data.
The project is intended as an embedded systems demonstration combining Zephyr drivers, sensors, GPIO control and multi-threaded application logic.
