
#include <Arduino.h>
#include <PromLokiTransport.h>
#include <PrometheusArduino.h>
#include <config.h>
#include <certificates.h>
#include <tuple>

#ifdef heltec_wifi_kit32
#include "heltec.h"
#endif

PromLokiTransport transport;
PromClient client(transport);

String wifi_status;
bool vibration_detected_count = 0;
bool motion_detected = false;
bool coffe_count_increased = false;
int16_t buffer_loop_count = 0;
int16_t coffe_count_current = 0;
int64_t coffe_count_total = 0;
int64_t current_time = 0;
int64_t motion_detection_start = 0;
int64_t last_metric_ingestion = 0;
int64_t last_remote_write = 0;

// Function prototypes
bool detectMotion();
void updateDisplay();
String performRemoteWrite();
void handleCoffeCountIncrease();
bool handleMotionBuffer();
void handleSampleIngestion();
void handleMetricsSend();

WriteRequest req(2, 1024);

// Define a TimeSeries which can hold up to 5 samples, has a name of `uptime_milliseconds`
TimeSeries ts1(5, "coffes_consumed", "{job=\"cmi_coffe_counter\",location=\"schwerzenbach_4OG\"}");

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    ;
  Serial.println("Starting up coffe counter ...");
  pinMode(VIBRATION_SENSOR_PIN, INPUT);

  #ifdef heltec_wifi_kit32
  Heltec.begin(true, false, true, true, 915E6);
  Heltec.display->setContrast(255);
  Heltec.display->clear();
  Heltec.display->drawString(0, 0, "Booting ...");
  Heltec.display->display();
  #endif

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

  req.addTimeSeries(ts1);
  if (DEBUG)
    Serial.println("Startup done");

  current_time = transport.getTimeMillis();
  last_metric_ingestion = current_time;
  last_remote_write = current_time;
  
  #ifdef heltec_wifi_kit32
  Heltec.display->clear();
  Heltec.display->drawString(0, 0, "Startup done");
  #endif
};

void loop()
{

  // return main loop as long as motion buffer is not full
  if (handleMotionBuffer() == false)
  {
    return;
  }

  if (DEBUG)
    Serial.println("Current motion status: " + String(motion_detected));

  current_time = transport.getTimeMillis();

  handleCoffeCountIncrease();

  handleSampleIngestion();

  handleMetricsSend();

  // Check if the wifi connection is still up and reconnect if necessary
  transport.checkAndReconnectConnection();

  // Update the display
  updateDisplay();
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
  if ((current_time - last_metric_ingestion) > (METRICS_INGESTION_RATE_SECONDS * 1000))
  {
    if (DEBUG)
      Serial.println("Ingesting metrics");
    if (!ts1.addSample(current_time, coffe_count_current))
    {
      if (DEBUG)
        Serial.println("Failed to add sample" + String(ts1.errmsg));
    }
    last_metric_ingestion = transport.getTimeMillis();
    coffe_count_current = 0;
  }
}

void handleCoffeCountIncrease()
{
  if (motion_detected == false)
  {
    motion_detection_start = 0;
    coffe_count_increased = false;
    return;
  }

  if (motion_detection_start == 0)
  {
    if (DEBUG)
      Serial.println("Motion detected, starting motion detection timer");
    motion_detection_start = transport.getTimeMillis();
    return;
  }

  if ((current_time - motion_detection_start) > (MOTION_DETECTION_DURATION_SECONDS * 1000))
  {
    if (coffe_count_increased)
    {
      if (DEBUG)
        Serial.println("Motion detected for more than " + String(MOTION_DETECTION_DURATION_SECONDS) + " seconds, but coffe count already increased, skipping");
      return;
    }
    coffe_count_current++;
    coffe_count_total++;
    coffe_count_increased = true;
    if (DEBUG)
    {
      Serial.println("Motion detected for more than " + String(MOTION_DETECTION_DURATION_SECONDS) + " seconds, increase coffe count");
      Serial.println("Current coffe count: " + String(coffe_count_current));
      Serial.println("Total coffe count: " + String(coffe_count_total));
    }
  }
}

bool handleMotionBuffer()
{
  buffer_loop_count++;
  bool motion = detectMotion();
  if (buffer_loop_count <= VIBRATION_BUFFER_LOOP_COUNT)
  {
    if (motion)
    {
      vibration_detected_count++;
    }
    return false;
  }

  buffer_loop_count = 0;
  motion_detected = false;
  if (vibration_detected_count >= VIBRATION_BUFFER_POSITIVE_THRESHOLD)
  {
    motion_detected = true;
  }
  vibration_detected_count = 0;
  return true;
};

String performRemoteWrite()
{
  Serial.println("Performing remote write");
  PromClient::SendResult res = client.send(req);
  if (!res == PromClient::SendResult::SUCCESS)
  {
    Serial.println(client.errmsg);
    return "error";
  }
  ts1.resetSamples();
  return "success";
}

bool detectMotion()
{
  bool motion_detected = false;
  if (digitalRead(VIBRATION_SENSOR_PIN) == HIGH)
  {
    motion_detected = true;
  }
  if (DEBUG)
  {
    Serial.println("Motion detected: " + String(motion_detected));
  }
  return motion_detected;
}

void updateDisplay()
{
  String motion_status = motion_detected ? "vibrating" : "not vibrating";

  #ifdef heltec_wifi_kit32
  Heltec.display->clear();
  Heltec.display->drawString(0, 0, "Coffee Count Current: " + String(coffe_count_current));
  Heltec.display->drawString(0, 10, "Coffee Count Total: " + String(coffe_count_total));
  Heltec.display->drawString(0, 20, "Vibration: " + motion_status);
  Heltec.display->display();
  #endif
}