
#include <Arduino.h>
#include <PromLokiTransport.h>
#include <PrometheusArduino.h>
#include <config.h>
#include <tuple>
#include <stdio.h>
#include <cstring>
#include <vibration_detect.h>
#include <transport.h>
#include <prometheus_histogramm.h>

Transport transport(WIFI_STATUS_LED_VCC, WIFI_SSID, WIFI_PASSWORD);

// Setup time variables
int64_t start_time;
int64_t current_time;
int64_t last_metric_ingestion;
int64_t last_remote_write;

// timer 0 of esp32 for vibration detection
hw_timer_t *timer0 = NULL;
// background task for vibration detection
TaskHandle_t vibration_detection_task;
// counter for the detected vibrations
SemaphoreHandle_t coffee_counters_sem;

// Function prototypes
bool detectMotion();
bool performRemoteWrite();
bool handleMotionBuffer();
void handleSampleIngestion();
void handleMetricsSend();
void ingestMetricSample(TimeSeries &ts, int64_t timestamp, int64_t value, String name);

// The write request that will be used to send the metrics to Prometheus. For every Histogramm you need to add 2 + number of buckets timeseries
WriteRequest req(16, 8192);

char *labels = "{job=\"cmi_coffee_counter\",location=\"schwerzenbach_4OG\"}";

// TimeSeries that can hold 5 samples each. Make sure to set sample_ingestation rate and remote_write_interval accordingly
Prometheus_Histogramm coffees_consumed("coffees_consumed", labels, 10, 10000, 4000, 10);
TimeSeries system_memory_free_bytes(10, "system_memory_free_bytes", labels);
TimeSeries system_memory_total_bytes(10, "system_memory_total_bytes", labels);
TimeSeries system_network_wifi_rssi(10, "system_network_wifi_rssi", labels);
TimeSeries system_largest_heap_block_size_bytes(10, "system_largest_heap_block_size_bytes", labels);

void setup()
{
  pinMode(VIBRATION_SENSOR_PIN, INPUT);
  pinMode(VIBRATION_DETECTION_LED_VCC, OUTPUT);
  pinMode(WIFI_STATUS_LED_VCC, OUTPUT);

  // Set all time variables to the current startup time
  start_time = transport.getTimeMillis();
  current_time = start_time;
  last_metric_ingestion = start_time;
  last_remote_write = start_time;

  // Setup serial
  Serial.begin(SERIAL_BAUD);
  while (!Serial)
    ;

  Serial.println("Starting up coffe counter ...");
  Serial.println("WiFi SSID: " + String(WIFI_SSID));

  // setup background task for vibration detection with semaphore
  timer0 = timerBegin(0, 80, true);
  coffee_counters_sem = xSemaphoreCreateBinary();
  xSemaphoreGive(coffee_counters_sem);

  // init coffees_consumed histogramm
  coffees_consumed.init(&req);

  // setup vibration detection task with parameters
  vibration_detection_parameters parameters;
  parameters.vibration_detection_threshold_ms = MOTION_DETECTION_DURATION_THREASHOLD_SECONDS * 1000;
  parameters.timer = timer0;
  parameters.vibration_counter_sem = &coffee_counters_sem;
  parameters.coffees_consumed = &coffees_consumed;
  create_vibration_dection_task(&vibration_detection_task, &parameters);

  // setup transportation to Grafana Cloud
  transport.setEndpoint(GC_PORT, GC_URL, (char *)GC_PATH);
  transport.setCredentials(GC_USER, GC_PASS);
  if (DEBUG)
  {
    transport.setDebug(Serial);
  }
  transport.beginAsync();

  // Add system metrics to the write request
  req.addTimeSeries(system_memory_free_bytes);
  req.addTimeSeries(system_memory_total_bytes);
  req.addTimeSeries(system_network_wifi_rssi);
  req.addTimeSeries(system_largest_heap_block_size_bytes);

  if (DEBUG)
    Serial.println("Startup done");
};

void loop()
{
  current_time = transport.getTimeMillis();

  handleSampleIngestion();
  // handleMetricsSend();

  vTaskDelay(50 / portTICK_PERIOD_MS);
}

void handleMetricsSend()
{
  if ((current_time - last_remote_write) > (REMOTE_WRITE_INTERVAL_SECONDS * 1000))
  {
    if (DEBUG)
      Serial.println("Performing remote write");
    bool success = performRemoteWrite();
    if (DEBUG)
      Serial.println("Remote Write successfull: " + String(success));
    last_remote_write = transport.getTimeMillis();
  }
}

void handleSampleIngestion()
{
  if ((current_time - last_metric_ingestion) <= (METRICS_INGESTION_RATE_SECONDS * 1000))
  {
    return;
  }
  if (DEBUG)
    Serial.println("Ingesting metrics");
  if (xSemaphoreTake(coffee_counters_sem, (TickType_t)10) == pdTRUE)
  {
    coffees_consumed.Ingest(current_time);

    xSemaphoreGive(coffee_counters_sem);
  }
  ingestMetricSample(system_memory_free_bytes, current_time, ESP.getFreeHeap(), "free_heap_bytes");
  ingestMetricSample(system_memory_total_bytes, current_time, ESP.getHeapSize(), "total_heap_bytes");
  ingestMetricSample(system_network_wifi_rssi, current_time, WiFi.RSSI(), "wifi_rssi");
  ingestMetricSample(system_largest_heap_block_size_bytes, current_time, ESP.getMaxAllocHeap(), "largest_heap_block_bytes");
  last_metric_ingestion = transport.getTimeMillis();
}

void ingestMetricSample(TimeSeries &ts, int64_t timestamp, int64_t value, String name)
{
  if (ts.addSample(timestamp, value))
  {
    if (DEBUG)
      Serial.println("Ingesting metrics for " + name + ": " + String(value) + " at " + String(timestamp));
  }
  else
  {
    if (DEBUG)
      Serial.println("Ingesting metrics: Failed to add sample" + String(ts.errmsg));
  }
}

bool performRemoteWrite()
{
  Serial.println("Performing remote write");
  PromClient::SendResult res = transport.send(req);
  if (!res == PromClient::SendResult::SUCCESS)
  {
    return false;
  }
  coffees_consumed.resetSamples();
  system_memory_free_bytes.resetSamples();
  system_memory_total_bytes.resetSamples();
  system_network_wifi_rssi.resetSamples();
  system_largest_heap_block_size_bytes.resetSamples();
  return true;
}