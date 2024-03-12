#include "transport.h"

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

void Transport::beginAsync()
{
    if (connectTaskHandle != NULL)
    {
        vTaskDelete(connectTaskHandle);
    }
    if (blinkTaskHandle != NULL)
    {
        vTaskDelete(blinkTaskHandle);
        blinkTaskHandle = NULL;
    }

    digitalWrite(wifiStatusPin, LOW);
    xTaskCreatePinnedToCore(
        Transport::connectTask,
        "transport connect",
        10000, /* Stack size in words */
        this,
        3, /* Priority of the task */
        &connectTaskHandle,
        tskNO_AFFINITY);
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
        wl_status_t wifiStatus = WiFi.status();
        if (wifiStatus == WL_CONNECTED)
        {
            int8_t dbm = WiFi.RSSI();
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
                instance->promTransport.checkAndReconnectConnection();
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
