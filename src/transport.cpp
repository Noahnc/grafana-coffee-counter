#include "transport.h"
#include "esp_wifi.h"

Transport::Transport(const int wifi_status_pin, const char *wifi_ssid, const char *wifi_password)
    : wifiStatusPin(wifi_status_pin), wifiSSID(wifi_ssid), wifiPassword(wifi_password)
{
    promTransport = PromLokiTransport();
    promClient = PromClient(promTransport);
    promTransport.setUseTls(true);
    promTransport.setCerts(grafanaCert, strlen(grafanaCert));
    promTransport.setWifiSsid(wifiSSID);
    promTransport.setWifiPass(wifiPassword);
    semaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(semaphore);
}

Transport::~Transport()
{
    if (connectTaskHandle != NULL)
    {
        vTaskDelete(connectTaskHandle);
        connectTaskHandle = NULL;
    }
    if (blinkTaskHandle != NULL)
    {
        vTaskDelete(blinkTaskHandle);
        blinkTaskHandle = NULL;
    }
    delete &promClient;
    delete &promTransport;
    vSemaphoreDelete(semaphore);
    digitalWrite(wifiStatusPin, LOW);
}

void Transport::setDebug(Stream &stream)
{
    promTransport.setDebug(stream);
    promClient.setDebug(stream);
    debug = &stream;
}

void Transport::setEndpoint(uint16_t port, const char *host, char *path)
{
    promClient.setPort(port);
    promClient.setUrl(host);
    promClient.setPath(path);
}

void Transport::setCredentials(const char *user, const char *pass)
{
    promClient.setUser(user);
    promClient.setPass(pass);
}

bool Transport::isInitialized()
{
    bool result = false;
    if (xSemaphoreTake(semaphore, (TickType_t)10) == pdTRUE)
    {
        result = transportInitialized;
        xSemaphoreGive(semaphore);
    }
    return result;
}

int64_t Transport::getTimeMillis()
{
    int64_t result = -1;
    while (result < 0)
    {
        if (xSemaphoreTake(semaphore, (TickType_t)50) == pdTRUE)
        {
            if (transportInitialized)
            {
                result = promTransport.getTimeMillis();
            }
            xSemaphoreGive(semaphore);
        }
    }
    return result;
}

PromClient::SendResult Transport::send(WriteRequest &req)
{
    PromClient::SendResult res = promClient.send(req);
    if (!res == PromClient::SendResult::SUCCESS)
    {
        Serial.println(promClient.errmsg);
    }
    return res;
}

void Transport::stop()
{
    if (connectTaskHandle != NULL)
    {
        vTaskDelete(connectTaskHandle);
        connectTaskHandle = NULL;
    }
    if (blinkTaskHandle != NULL)
    {
        vTaskDelete(blinkTaskHandle);
        blinkTaskHandle = NULL;
    }
    digitalWrite(wifiStatusPin, LOW);
}

int8_t Transport::lastWifiRssi()
{
    return last_wiffi_rssi_value;
}

void Transport::beginAsync()
{
    if (connectTaskHandle != NULL)
    {
        return;
    }
    xTaskCreatePinnedToCore(
        Transport::connectTask,
        "transport connect",
        10000, /* Stack size in words */
        this,
        3, /* Priority of the task */
        &connectTaskHandle,
        tskNO_AFFINITY);
}

bool Transport::awaitConnectedWifi(uint16_t timeout_sec)
{
    const uint32_t task_delay_sec = 1;
    uint16_t max_wait_cycles = timeout_sec / task_delay_sec;
    uint16_t current_wait_cycle = 0;
    while (WiFi.status() != WL_CONNECTED && current_wait_cycle < max_wait_cycles)
    {
        vTaskDelay((task_delay_sec * 1000) / portTICK_PERIOD_MS);
        Serial.print('.');
        current_wait_cycle++;
    }
    return WiFi.status() == WL_CONNECTED;
}

void Transport::connectTask(void *args)
{
    Transport *instance = static_cast<Transport *>(args);

    while (true)
    {
        // ensure init
        if (xSemaphoreTake(instance->semaphore, (TickType_t)1000) == pdTRUE)
        {
            try
            {
                if (!instance->transportInitialized)
                {
                    instance->startLedBlink(StatusIndicator::Connecting);
                    if (!instance->promTransport.begin())
                    {
                        Serial.println(instance->promTransport.errmsg);
                    }
                    else if (!instance->promClient.begin())
                    {
                        Serial.println(instance->promClient.errmsg);
                    }
                    else
                    {
                        instance->transportInitialized = true;
                    }

                    xSemaphoreGive(instance->semaphore);
                    vTaskDelay(5000 / portTICK_PERIOD_MS);
                    continue;
                }
            }
            catch (const std::exception &e)
            {
                Serial.println(e.what());
                xSemaphoreGive(instance->semaphore);
                vTaskDelay(5000 / portTICK_PERIOD_MS);
                continue;
            }
            xSemaphoreGive(instance->semaphore);
        }
        else
        {
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            continue;
        }

        // update LED status and try to reconnect if required
        if (WiFi.status() == WL_CONNECTED)
        {
            int8_t dbm = WiFi.RSSI();
            instance->last_wiffi_rssi_value = dbm;
            if (instance->debug != nullptr)
            {
                instance->debug->println("Wifi Signal: " + String(dbm) + "dBm");
            }
            if (dbm > -70)
            {
                // good/fair connection
                instance->stopLedBlink();
                digitalWrite(instance->wifiStatusPin, HIGH);
            }
            else
            {
                // bad connection
                instance->startLedBlink(StatusIndicator::ConnectedBadSignal);
            }
        }
        else
        {
            instance->startLedBlink(StatusIndicator::Connecting);
            try
            {
                const int16_t timeout_sec = 1800;
                if (WiFi.status() != WL_CONNECTED)
                {
                    WiFi.disconnect();
                    yield();
                    // For some reason, ESP32 can't sometimes reconnect wifi after light sleep.
                    // Calling esp_wifi_stop before and esp_wifi_start after light sleep seems prevent this issue
                    esp_wifi_stop();
                    yield();
                    esp_wifi_start();
                    if (instance->debug != nullptr)
                    {
                        instance->debug->println("Connecting to " + String(instance->wifiSSID));
                    }

                    WiFi.mode(WIFI_STA);
                    WiFi.begin(instance->wifiSSID, instance->wifiPassword);
                    if (awaitConnectedWifi(timeout_sec))
                    {
                        if (instance->debug != nullptr)
                        {
                            instance->debug->println("Connected. IP is " + WiFi.localIP().toString());
                        }
                    }
                    else
                    {
                        Serial.println("Failed to connect to Wifi after " + String(timeout_sec) + ", rebooting.");
                        ESP.restart();
                    }
                }
            }
            catch (const std::exception &e)
            {
                Serial.println(e.what());
            }
        }
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

void Transport::startLedBlink(StatusIndicator statusIndicator)
{
    blinkIntervalMs = static_cast<int>(statusIndicator);
    if (blinkTaskHandle != NULL)
    {
        // blink task already running
        return;
    }
    xTaskCreatePinnedToCore(
        blinkLedTask,
        "Wifi LED blink",
        10000, /* Stack size in words */
        this,
        1, /* Priority of the task */
        &blinkTaskHandle,
        tskNO_AFFINITY);
}

void Transport::stopLedBlink()
{
    if (blinkTaskHandle != NULL)
    {
        vTaskDelete(blinkTaskHandle);
        blinkTaskHandle = NULL;
    }
}

void Transport::blinkLedTask(void *args)
{
    Transport *instance = static_cast<Transport *>(args);
    while (true)
    {
        digitalWrite(instance->wifiStatusPin, HIGH);
        vTaskDelay(instance->blinkIntervalMs / portTICK_PERIOD_MS);
        digitalWrite(instance->wifiStatusPin, LOW);
        vTaskDelay(instance->blinkIntervalMs / portTICK_PERIOD_MS);
    }
}
