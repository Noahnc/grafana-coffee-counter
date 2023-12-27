# Grafana Coffe Counter

ToDo: Add documentation

## Secrets

Das Projekt benötigt folgende Secrets, die in einer Datei `user_config.ini` im Projektverzeichnis abgelegt oder als Umgebungsvariable definiert werden müssen:

```INI
[user_config]
build_flags =
	-D ENV_GRAFANA_USER=\"<Grafana user>\"
	-D ENV_GRAFANA_PASSWORD=\"<Grafana password>=\"
	-D ENV_WIFI_SSID=\"<Wifi SSID>\"
	-D ENV_WIFI_PASSWORD=\"<Wifi password>!\"
```
