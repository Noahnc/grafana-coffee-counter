#include "vibration.h"

Vibration::Vibration(hw_timer_t *timer, int32_t vibration_detection_threshold_ms, Prometheus_Histogram *coffees_consumed)
{
    Vibration::timer = timer;
    Vibration::vibration_detection_threshold_ms = vibration_detection_threshold_ms;
    Vibration::coffees_consumed = coffees_consumed;
}

Vibration::~Vibration()
{
    if (vibration_detection_task != NULL){
        vTaskDelete(vibration_detection_task);
    }
    timerStop(timer);
}

void Vibration::beginAsync()
{
    if (vibration_detection_task == NULL)
    {
        Serial.println("Starting vibration detection");
        xTaskCreatePinnedToCore(
            Vibration::vibration_dection_task,
            "vibration detection",
            10000, /* Stack size in words */
            this,
            3, /* Priority of the task */
            &vibration_detection_task,
            tskNO_AFFINITY);
    }
}

void Vibration::vibration_dection_task(void *args)
{
    Vibration *instance = static_cast<Vibration *>(args);
    while (true)
    {
        int64_t duration_ms = detect_vibration(instance->timer);
        if (duration_ms >= instance->vibration_detection_threshold_ms)
        {
            if (DEBUG)
            {
                Serial.println("Vibration detected (" + String(duration_ms) + " ms)");
            }
            instance->coffees_consumed->AddValue(duration_ms);
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

/// @brief Reads the vibration sensor an returns the duration of the detected vibration.
/// @return Duration of detected vibration.
int64_t Vibration::detect_vibration(hw_timer_t *timer)
{
    timerWrite(timer, 0);

    while (digitalRead(VIBRATION_SENSOR_PIN) == LOW)
    {
        timerStart(timer);
        digitalWrite(VIBRATION_DETECTION_LED_VCC, HIGH);
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }

    timerStop(timer);
    digitalWrite(VIBRATION_DETECTION_LED_VCC, LOW);

    int64_t vibration_duration_ms = timerReadMilis(timer);

    if (DEBUG && vibration_duration_ms > 1000)
    {
        Serial.println("Vibration " + String(vibration_duration_ms));
    }

    return vibration_duration_ms;
}