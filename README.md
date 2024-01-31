# Grafana Coffee Counter

This project contains all resources to build a Grafana Cloud Coffee Counter.
The coffee counter consists of a custom PCB with an ESP32 and an DFRobot Vibration Sensor.

## How it works

The vibration sensor is connected to the ESP32 and reads the vibration state. If vibration is detected, the ESP32 will count the amount of time the vibration sensor is continuously active. If the vibration sensor is active for more than 8 seconds, the vibration is consideres as a coffee and counters of a Prometheus histogram are increased. Every 60s, a new Time Series is created for the coffee histogram and some other system metrics. The data is then sent to Grafana Cloud Mimir using Prometheus Remote Write. Since the library used for the Prometheus Remote Write is fully Prometheus compatible, the data can technically also be sent to any other Prometheus compatible system. Just make sure to change the URL and the root certificate accordingly.

## Hardware

The following hardware is used for this project:

- https://wiki.dfrobot.com/Vibration_Sensor_SKU_SEN0433
- ESP32 WROVER-E
- Custom PCB (see `./easy_eda`)

## Build Flags

To build this project, you have to create a file called `user_config.ini` in the Project root directory. This file contains the following build flags:

```INI
[user_config]
build_flags =
	-D ENV_GRAFANA_USER=\"<Grafana user>\"
	-D ENV_GRAFANA_PASSWORD=\"<Grafana password>=\"
	-D ENV_WIFI_SSID=\"<Wifi SSID>\"
	-D ENV_WIFI_PASSWORD=\"<Wifi password>!\"
	-D ENV_LABEL_SITE=\"<label value for label SITE>\"
	-D ENV_LABEL_FLOOR=\"<labe value for label FLOOR>\"
```

### Hardware & Schema

The design of the custom PCB can be found in the folder `./easy_eda`. The design was created with [EasyEDA](https://easyeda.com/).
In addition to the custom PCB, there are STL files for a 3D-printed case in the folder `./3d_print`.

## CP2102N: USB to UART

The USB-to-UART chip `CP2102N` used in the schema under `./easy_eda` is configured using the [Simplicity Studio Software](https://www.silabs.com/developers/simplicity-studio) from Silicon Labs.

In `./cp2102n_configuration` there is a configuration file that can be played onto the chip with Simplicity Studio Software to set the GPIO configuration for the LED `TXT` and `RXT` and to set device information for the USB protocol. See also [Application Note AN721](https://www.silabs.com/documents/public/application-notes/AN721.pdf) for instructions.

Under Linux it may be necessary to set the execute flag manually for some files:

`chmod -x ./offline/ffd/xpress_configurator/cp210x_base/bin/Linux/*`

In addition, it should be checked whether all dependencies are available:

`ldd ./offline/ffd/xpress_configurator/cp210x_base/bin/Linux/cp210xcfg`
