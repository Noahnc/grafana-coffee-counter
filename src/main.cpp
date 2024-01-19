
#include <Arduino.h>
#include <PromLokiTransport.h>
#include <PrometheusArduino.h>
#include <config.h>
#include <tuple>

#ifdef heltec_wifi_kit32
#include "heltec.h"
#endif

#include <vibration_detect.h>
#include <transport.h>

Transport transport(WIFI_STATUS_LED_VCC, WIFI_SSID, WIFI_PASSWORD);

int64_t coffee_count_total = 0;
int64_t current_time = 0;
int64_t last_metric_ingestion = 0;
int64_t last_remote_write = 0;

// timer 0 of esp32 for vibration detection
hw_timer_t *timer0 = NULL;
// background task for vibration detection
TaskHandle_t vibration_detection_task;
// counter for the detected vibrations
SemaphoreHandle_t vibration_counter_sem;

// Function prototypes
bool detectMotion();
void updateDisplay();
String performRemoteWrite();
bool handleMotionBuffer();
void handleSampleIngestion();
void handleMetricsSend();
void ingestMetricSample(TimeSeries &ts, int64_t timestamp, int64_t value);

WriteRequest req(2, 1024);

const char *labels = "{job=\"cmi_coffee_counter\",location=\"schwerzenbach_4OG\"}";
// TimeSeries that can hold 5 samples each. Make sure to set sample_ingestation rate and remote_write_interval accordingly
TimeSeries coffees_consumed(5, "coffees_consumed_counter", labels);
TimeSeries system_memory_free_bytes(5, "system_memory_free_bytes", labels);
TimeSeries system_memory_total_bytes(5, "system_memory_total_bytes", labels);
TimeSeries system_network_wifi_rssi(5, "system_network_wifi_rssi", labels);
TimeSeries system_largest_heap_block_size_bytes(5, "system_largest_heap_block_size_bytes", labels);

void setup()
{
  pinMode(VIBRATION_SENSOR_PIN, INPUT);
  pinMode(VIBRATION_DETECTION_LED_VCC, OUTPUT);
  pinMode(WIFI_STATUS_LED_VCC, OUTPUT);

  Serial.begin(SERIAL_BAUD);
  while (!Serial)
    ;

  Serial.println("Starting up coffe counter ...");
  Serial.println("WiFi SSID: " + String(WIFI_SSID));

  // setup background task for vibration detection
  timer0 = timerBegin(0, 80, true);
  vibration_counter_sem = xSemaphoreCreateBinary();
  xSemaphoreGive(vibration_counter_sem);
  vibration_detection_parameters parameters;
  parameters.detection_threshold_ms = MOTION_DETECTION_DURATION_SECONDS * 1000;
  parameters.timer = timer0;
  parameters.vibration_counter_sem = &vibration_counter_sem;
  parameters.vibration_counter = &coffee_count_total;
  create_vibration_dection_task(&vibration_detection_task, &parameters);

#ifdef heltec_wifi_kit32
  Heltec.begin(true, false, true, true, 915E6);
  Heltec.display->setContrast(255);
  Heltec.display->clear();
  Heltec.display->drawString(0, 0, "Booting ...");
  Heltec.display->display();
#endif

  // setup transportation
  transport.setEndpoint(GC_PORT, GC_URL, (char *)GC_PATH);
  transport.setCredentials(GC_USER, GC_PASS);
  if (DEBUG)
  {
    transport.setDebug(Serial);
  }
  transport.beginAsync();

  req.addTimeSeries(coffees_consumed);
  req.addTimeSeries(system_memory_free_bytes);
  req.addTimeSeries(system_memory_total_bytes);
  req.addTimeSeries(system_network_wifi_rssi);
  req.addTimeSeries(system_largest_heap_block_size_bytes);
  if (DEBUG)
    Serial.println("Startup done");

  current_time = 0;
  last_metric_ingestion = 0;
  last_remote_write = 0;

#ifdef heltec_wifi_kit32
  Heltec.display->clear();
  Heltec.display->drawString(0, 0, "Startup done");
#endif
};

void loop()
{
  current_time = transport.getTimeMillis();

  handleSampleIngestion();
  handleMetricsSend();

  // Update the display
  updateDisplay();

  vTaskDelay(50 / portTICK_PERIOD_MS);
}

void handleMetricsSend()
{
  if ((current_time - last_remote_write) > (REMOTE_WRITE_INTERVAL_SECONDS * 1000))
  {
    if (DEBUG)
      Serial.println("Performing remote write");
    String res = performRemoteWrite();
    if (DEBUG)
      Serial.println("Remote write result: " + res);
    last_remote_write = transport.getTimeMillis();
  }
}

void handleSampleIngestion()
{
  if ((current_time - last_metric_ingestion) <= (METRICS_INGESTION_RATE_SECONDS * 1000))
  {
    return;
  }
  if (xSemaphoreTake(vibration_counter_sem, (TickType_t)10) == pdTRUE)
  {
    Serial.println("Total coffee count: " + String(coffee_count_total));
    ingestMetricSample(coffees_consumed, current_time, coffee_count_total);

    xSemaphoreGive(vibration_counter_sem);
  }
  ingestMetricSample(system_memory_free_bytes, current_time, ESP.getFreeHeap());
  ingestMetricSample(system_memory_total_bytes, current_time, ESP.getHeapSize());
  ingestMetricSample(system_network_wifi_rssi, current_time, WiFi.RSSI());
  ingestMetricSample(system_largest_heap_block_size_bytes, current_time, ESP.getMaxAllocHeap());
  last_metric_ingestion = transport.getTimeMillis();
}

void ingestMetricSample(TimeSeries &ts, int64_t timestamp, int64_t value)
{
  if (ts.addSample(timestamp, value))
  {
    if (DEBUG)
    {
      Serial.println("Ingesting metrics: Sample added");
    }
  }
  else
  {
    if (DEBUG)
    {
      Serial.println("Ingesting metrics: Failed to add sample" + String(ts.errmsg));
    }
  }
}

String performRemoteWrite()
{
  Serial.println("Performing remote write");
  PromClient::SendResult res = transport.send(req);
  if (!res == PromClient::SendResult::SUCCESS)
  {
    return "error";
  }
  coffees_consumed.resetSamples();
  system_memory_free_bytes.resetSamples();
  system_memory_total_bytes.resetSamples();
  system_network_wifi_rssi.resetSamples();
  system_largest_heap_block_size_bytes.resetSamples();
  return "success";
}

void updateDisplay()
{
#ifdef heltec_wifi_kit32
  if (xSemaphoreTake(vibration_counter_sem, (TickType_t)10) == pdTRUE)
  {
    Heltec.display->drawString(0, 0, "Coffee Total Count: " + String(coffee_count_total));
    Heltec.display->display();
    xSemaphoreGive(vibration_counter_sem);
  }
#endif
}