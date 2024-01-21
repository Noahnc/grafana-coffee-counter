# Grafana Coffee Counter

ToDo: Add documentation

Hardware:
* https://wiki.dfrobot.com/Vibration_Sensor_SKU_SEN0433
* ESP32 WROVER-E
* Custom PCB

## Secrets

Das Projekt benötigt folgende Secrets, die in einer Datei `user_config.ini` im Projektverzeichnis abgelegt oder als Umgebungsvariable definiert werden müssen:

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

Ein Schema- und PCB-Design für das Projekt befinden sich im Verzeichnis `./easy_eda`, die mit https://easyeda.com bearbeitet werden können.

## CP2102N: USB to UART

Der im Schema unter `./easy_eda` verwendete USB-to-UART-Chip `CP2102N` wird über das [Simplicity Studio Software](https://www.silabs.com/developers/simplicity-studio) von Silicon Labs konfiguriert.

In `./cp2102n_configuration` befindet sich eine Konfigurationsdatei, die mit Simplicity Studio Software auf den Chip gespielt werden kann, um die GPIO-Konfiguration für die LED `TXT` und `RXT` zu setzen und Device Informationen für das USB Protokoll zu setzen. Siehe auch [Application Note AN721](https://www.silabs.com/documents/public/application-notes/AN721.pdf) für eine Anleitung.

Unter Linux ist es möglicherweise erforderlich, bei einigen Dateien das Execute-Flag von Hand zu setzen:

`chmod -x ./offline/ffd/xpress_configurator/cp210x_base/bin/Linux/*`

Zudem sollte überprüft werden, ob alle Dependencies verfügbar sind:

`ldd ./offline/ffd/xpress_configurator/cp210x_base/bin/Linux/cp210xcfg`
