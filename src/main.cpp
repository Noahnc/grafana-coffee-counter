
#include <Arduino.h>
#include <PromLokiTransport.h>
#include <PrometheusArduino.h>
#include <config.h>
#include <certificates.h>
#include <tuple>
#include <stdio.h>
#include <cstring>
#include "vibration_detect.h"

PromLokiTransport transport;
PromClient client(transport);

String wifi_status;
int64_t small_coffee_count = 0;
int64_t medium_coffee_count = 0;
int64_t large_coffee_count = 0;
int64_t current_time = 0;
int64_t last_metric_ingestion = 0;
int64_t last_remote_write = 0;

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

WriteRequest req(2, 1024);

char *labels = "{job=\"cmi_coffee_counter\",location=\"schwerzenbach_4OG\"}";

// TimeSeries that can hold 5 samples each. Make sure to set sample_ingestation rate and remote_write_interval accordingly
TimeSeries coffees_consumed_small(5, "coffees_consumed_count_total", "job=\"cmi_coffee_counter\",location=\"schwerzenbach_4OG\",coffee_size=\"small\"}");
TimeSeries coffees_consumed_medium(5, "coffees_consumed_count_total", "job=\"cmi_coffee_counter\",location=\"schwerzenbach_4OG\",coffee_size=\"medium\"}");
TimeSeries coffees_consumed_large(5, "coffees_consumed_count_total", "job=\"cmi_coffee_counter\",location=\"schwerzenbach_4OG\",coffee_size=\"large\"}");
TimeSeries system_memory_free_bytes(5, "system_memory_free_bytes", labels);
TimeSeries system_memory_total_bytes(5, "system_memory_total_bytes", labels);
TimeSeries system_network_wifi_rssi(5, "system_network_wifi_rssi", labels);
TimeSeries system_largest_heap_block_size_bytes(5, "system_largest_heap_block_size_bytes", labels);

void setup()
{
  Serial.begin(SERIAL_BAUD);
  while (!Serial)
    ;
  Serial.println("Starting up coffe counter ...");
  Serial.println("WiFi SSID: " + String(WIFI_SSID));

  pinMode(VIBRATION_SENSOR_PIN, INPUT);
  pinMode(VIBRATION_DETECTION_LED_VCC, OUTPUT);

  // setup background task for vibration detection
  timer0 = timerBegin(0, 80, true);
  coffee_counters_sem = xSemaphoreCreateBinary();
  xSemaphoreGive(coffee_counters_sem);
  vibration_detection_parameters parameters;
  parameters.detection_threshold_ms_small_coffee = MOTION_DETECTION_DURATION_SECONDS_SMALL_COFFEE * 1000;
  parameters.detection_threshold_ms_medium_coffee = MOTION_DETECTION_DURATION_SECONDS_MEDIUM_COFFEE * 1000;
  parameters.detection_threshold_ms_large_coffee = MOTION_DETECTION_DURATION_SECONDS_LARGE_COFFEE * 1000;
  parameters.timer = timer0;
  parameters.vibration_counter_sem = &coffee_counters_sem;
  parameters.vibration_counter_small_coffee = &small_coffee_count;
  parameters.vibration_counter_medium_coffee = &medium_coffee_count;
  parameters.vibration_counter_large_coffee = &large_coffee_count;
  create_vibration_dection_task(&vibration_detection_task, &parameters);

  transport.setUseTls(true);
  transport.setCerts(grafanaCert, strlen(grafanaCert));
  transport.setWifiSsid(WIFI_SSID);
  transport.setWifiPass(WIFI_PASSWORD);
  transport.setDebug(Serial); // Remove this line to disable debug logging of the client.
  if (!transport.begin())
  {
    Serial.println(transport.errmsg);
    while (true)
    {
    };
  }

  // Configure the client
  client.setUrl(GC_URL);
  client.setPath((char *)GC_PATH);
  client.setPort(GC_PORT);
  client.setUser(GC_USER);
  client.setPass(GC_PASS);
  if (DEBUG)
  {
    client.setDebug(Serial);
  }
  if (!client.begin())
  {
    Serial.println(client.errmsg);
    while (true)
    {
    };
  }

  req.addTimeSeries(coffees_consumed_small);
  req.addTimeSeries(coffees_consumed_medium);
  req.addTimeSeries(coffees_consumed_large);
  req.addTimeSeries(system_memory_free_bytes);
  req.addTimeSeries(system_memory_total_bytes);
  req.addTimeSeries(system_network_wifi_rssi);
  req.addTimeSeries(system_largest_heap_block_size_bytes);
  if (DEBUG)
    Serial.println("Startup done");

  current_time = transport.getTimeMillis();
  last_metric_ingestion = current_time;
  last_remote_write = current_time;
};

void loop()
{
  current_time = transport.getTimeMillis();

  handleSampleIngestion();
  handleMetricsSend();

  // Check if the wifi connection is still up and reconnect if necessary
  transport.checkAndReconnectConnection();

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
  if (xSemaphoreTake(coffee_counters_sem, (TickType_t)10) == pdTRUE)
  {
    ingestMetricSample(coffees_consumed_small, current_time, small_coffee_count, "small_coffee_count");
    ingestMetricSample(coffees_consumed_medium, current_time, medium_coffee_count, "medium_coffee_count");
    ingestMetricSample(coffees_consumed_large, current_time, large_coffee_count, "large_coffee_count");

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
  PromClient::SendResult res = client.send(req);
  if (!res == PromClient::SendResult::SUCCESS)
  {
    Serial.println(client.errmsg);
    return false;
  }
  coffees_consumed_small.resetSamples();
  coffees_consumed_medium.resetSamples();
  coffees_consumed_large.resetSamples();
  system_memory_free_bytes.resetSamples();
  system_memory_total_bytes.resetSamples();
  system_network_wifi_rssi.resetSamples();
  system_largest_heap_block_size_bytes.resetSamples();
  return true;
}