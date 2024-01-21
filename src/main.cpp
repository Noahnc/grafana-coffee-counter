
#include <Arduino.h>
#include <PromLokiTransport.h>
#include <PrometheusArduino.h>
#include <config.h>
#include <tuple>
#include <stdio.h>
#include <cstring>
#include <vibration.h>
#include <transport.h>
#include <prometheus_histogram.h>

// Increase stack size for the main loop since the default 8192 bytes are not enough
SET_LOOP_TASK_STACK_SIZE(32768);

// Function prototypes
bool performRemoteWrite();
void handleSampleIngestion();
void handleMetricsSend();
void ingestMetricSample(TimeSeries &ts, int64_t timestamp, int64_t value, String name);

// Setup time variables
int64_t start_time_unix_ms = 0;
int64_t run_time_ms = 0;
int64_t current_cicle_start_time_unix_ms = 0;
int64_t last_metric_ingestion_unix_ms = 0;
int64_t last_remote_write_unix_ms = 0;

// int to count remote write failures
int remote_write_failures = 0;

// The write request that will be used to send the metrics to Prometheus. For every Histogramm you need to add 3 + number of buckets timeseries
WriteRequest req(18, 4096);

char *labels = "{job=\"cmi_coffee_counter\",location=\"schwerzenbach_4OG\"}";

// TimeSeries that can hold 5 samples each. Make sure to set sample_ingestation rate and remote_write_interval accordingly
Prometheus_Histogram coffees_consumed("coffees_consumed", labels, 5, 10000, 4000, 10);
TimeSeries system_memory_free_bytes(5, "system_memory_free_bytes", labels);
TimeSeries system_memory_total_bytes(5, "system_memory_total_bytes", labels);
TimeSeries system_network_wifi_rssi(5, "system_network_wifi_rssi", labels);
TimeSeries system_largest_heap_block_size_bytes(5, "system_largest_heap_block_size_bytes", labels);
TimeSeries system_run_time_ms(5, "system_run_time_ms", labels);
TimeSeries system_remote_write_failures_count(5, "system_remote_write_failures_count", labels);

hw_timer_t *timer0 = timerBegin(0, 80, true); // timer 0 of esp32 for vibration detection
Vibration vibration(timer0, MOTION_DETECTION_DURATION_THREASHOLD_SECONDS * 1000, &coffees_consumed);

Transport transport(WIFI_STATUS_LED_VCC, WIFI_SSID, WIFI_PASSWORD);

void setup()
{
  pinMode(VIBRATION_SENSOR_PIN, INPUT);
  pinMode(VIBRATION_DETECTION_LED_VCC, OUTPUT);
  pinMode(WIFI_STATUS_LED_VCC, OUTPUT);

  // Setup serial
  Serial.begin(SERIAL_BAUD);
  while (!Serial)
    ;

  Serial.println("Starting up coffe counter ...");
  Serial.println("WiFi SSID: " + String(WIFI_SSID));

  // setup background task for vibration detection
  vibration.beginAsync();

  // init coffees_consumed histogram
  coffees_consumed.init(req);

  // setup transportation to Grafana Cloud
  transport.setEndpoint(GC_PORT, GC_URL, (char *)GC_PATH);
  transport.setCredentials(GC_USER, GC_PASS);
  if (DEBUG)
  {
    req.setDebug(Serial);
    transport.setDebug(Serial);
  }
  transport.beginAsync();

  // Add system metrics to the write request
  req.addTimeSeries(system_memory_free_bytes);
  req.addTimeSeries(system_memory_total_bytes);
  req.addTimeSeries(system_network_wifi_rssi);
  req.addTimeSeries(system_largest_heap_block_size_bytes);

  // Set all time variables to the current startup time
  start_time_unix_ms = transport.getTimeMillis();
  last_metric_ingestion_unix_ms = start_time_unix_ms;
  last_remote_write_unix_ms = start_time_unix_ms;

  if (DEBUG)
    Serial.println("Startup done");
};

void loop()
{
  current_cicle_start_time_unix_ms = transport.getTimeMillis();
  run_time_ms = current_cicle_start_time_unix_ms - start_time_unix_ms;

  handleSampleIngestion();
  handleMetricsSend();

  vTaskDelay(4000 / portTICK_PERIOD_MS);
}

void handleMetricsSend()
{
  int64_t next_remote_write_ms = last_remote_write_unix_ms + (REMOTE_WRITE_INTERVAL_SECONDS * 1000) - current_cicle_start_time_unix_ms;
  if (next_remote_write_ms > 0)
  {
    if (DEBUG)
      Serial.println("Next remote write in " + String(next_remote_write_ms / 1000) + " seconds");
    return;
  }

  if (DEBUG)
    Serial.println("Performing remote write");

  bool success = performRemoteWrite();
  if (!success)
  {
    remote_write_failures++;
    if (DEBUG)
      Serial.println("Remote Write failed: " + String(success));
    return;
  }
  if (DEBUG)
    Serial.println("Remote Write successful");
  last_remote_write_unix_ms = transport.getTimeMillis();
}

void handleSampleIngestion()
{
  int64_t next_ingestion_ms = last_metric_ingestion_unix_ms + (METRICS_INGESTION_RATE_SECONDS * 1000) - current_cicle_start_time_unix_ms;
  if (next_ingestion_ms > 0)
  {
    if (DEBUG)
      Serial.println("Next metric ingestion in " + String(next_ingestion_ms / 1000) + " seconds");
    return;
  }
  if (DEBUG)
    Serial.println("Ingesting metrics");

  coffees_consumed.Ingest(current_cicle_start_time_unix_ms);
  ingestMetricSample(system_memory_free_bytes, current_cicle_start_time_unix_ms, ESP.getFreeHeap(), "free_heap_bytes");
  ingestMetricSample(system_memory_total_bytes, current_cicle_start_time_unix_ms, ESP.getHeapSize(), "total_heap_bytes");
  ingestMetricSample(system_network_wifi_rssi, current_cicle_start_time_unix_ms, WiFi.RSSI(), "wifi_rssi");
  ingestMetricSample(system_largest_heap_block_size_bytes, current_cicle_start_time_unix_ms, ESP.getMaxAllocHeap(), "largest_heap_block_bytes");
  ingestMetricSample(system_run_time_ms, current_cicle_start_time_unix_ms, run_time_ms, "run_time_ms");
  ingestMetricSample(system_remote_write_failures_count, current_cicle_start_time_unix_ms, remote_write_failures, "remote_write_failures_count");
  last_metric_ingestion_unix_ms = transport.getTimeMillis();
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
  system_run_time_ms.resetSamples();
  system_remote_write_failures_count.resetSamples();
  return true;
}