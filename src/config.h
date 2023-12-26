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

// Duration in seconds for which motion has to be detected to count as a motion event
#define MOTION_DETECTION_DURATION_SECONDS 5

// Pin to which the vibration sensor is connected
#define VIBRATION_SENSOR_PIN 36
// Pin connected to a LED to indicated that vibration is detected
#define VIBRATION_DETECTION_LED_VCC 25

// Amount of loops to buffer a positive vibration signal.
#define VIBRATION_BUFFER_LOOP_COUNT 50

// Amount of positive vibration signals during the buffer loop to count as a motion event.
#define VIBRATION_BUFFER_POSITIVE_THRESHOLD 2