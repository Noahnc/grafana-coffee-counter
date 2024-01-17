#ifndef TRANSPORT_INCLUDED
#define TRANSPORT_INCLUDED

#include <certificates.h>
#include <Arduino.h>
#include <PrometheusArduino.h>
#include <PromLokiTransport.h>
#include <WiFi.h>


class Transport {
public:
    Transport(const int wifi_status_pin, const char* wifi_ssid, const char* wifi_password);
    void setEndpoint(uint16_t port, const char* host, char* path);
    void setCredentials(const char* user, const char* pass);
    void setDebug(Stream& stream);
    void beginAsync();
    bool isInitialized();
    int64_t getTimeMillis();
    PromClient::SendResult send(WriteRequest& req);

private:
    void startLedBlink();
    void stopLedBlink();

    static void blinkLedTask(void *args);
    static void connectTask(void *args);

    const char* wifiSSID;
    const char* wifiPassword;
    const int wifiStatusPin;
    PromLokiTransport promTransport;
    PromClient promClient;
    Stream* debug = nullptr;
    TaskHandle_t connectTaskHandle = NULL;
    TaskHandle_t blinkTaskHandle = NULL;
    SemaphoreHandle_t semaphore;
    bool transportInitialized = false;
};

#endif
