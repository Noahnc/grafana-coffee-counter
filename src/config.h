// Wifi config of the esp32
#define WIFI_SSID ENV_WIFI_SSID
#define WIFI_PASSWORD ENV_WIFI_PASSWORD

// For more information on where to get these values see: https://github.com/grafana/diy-iot/blob/main/README.md#sending-metrics
#define GC_URL "prometheus-prod-22-prod-eu-west-3.grafana.net"
#define GC_PATH "/api/prom/push"
#define GC_PORT 443
#define GC_USER ENV_GRAFANA_USER
#define GC_PASS ENV_GRAFANA_PASSWORD

// Enable debug logging to serial
#define DEBUG true
#define SERIAL_BAUD MONITOR_SPEED

// Interval in seconds between sending metrics to Grafana Cloud
#define REMOTE_WRITE_INTERVAL_SECONDS 180

// Metrics Ingestion Rate
#define METRICS_INGESTION_RATE_SECONDS 60

// Amount of time motion needs to be detected to count as a cup of small, medium or large coffee
#define MOTION_DETECTION_DURATION_SECONDS_SMALL_COFFEE 10
#define MOTION_DETECTION_DURATION_SECONDS_MEDIUM_COFFEE 18
#define MOTION_DETECTION_DURATION_SECONDS_LARGE_COFFEE 30

// Pin to which the vibration sensor is connected
#define VIBRATION_SENSOR_PIN 36
// Pin connected to a LED to indicated that vibration is detected
#define VIBRATION_DETECTION_LED_VCC 25

// Pin to indicate WIFI status
#define WIFI_STATUS_LED_VCC 26