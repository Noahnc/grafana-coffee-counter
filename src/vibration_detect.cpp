#include "vibration_detect.h"

/// @brief Reads the vibration sensor an returns the duration of the detected vibration.
/// @return Duration of detected vibration or -1 if threshold not reached.
int64_t detect_vibration(hw_timer_t *timer, int32_t detection_threshold_ms)
{
    timerWrite(timer, 0);

    while (digitalRead(VIBRATION_SENSOR_PIN) == LOW)
    {
        timerStart(timer);
        digitalWrite(VIBRATION_DETECTION_LED_VCC, HIGH);
        // The while loop may cause 100% cpu load as long as vibration is detected.
        // Delay the task between measures to allow other tasks to run.
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }

    timerStop(timer);
    digitalWrite(VIBRATION_DETECTION_LED_VCC, LOW);

    int64_t vibration_duration_ms = timerReadMilis(timer);

    if (DEBUG && vibration_duration_ms > 1)
    {
        Serial.println("Vibration " + String(vibration_duration_ms));
    }

    if (vibration_duration_ms >= detection_threshold_ms)
    {
        return vibration_duration_ms;
    }
    return -1;
}

void vibration_dection_task(void *args)
{
    vibration_detection_parameters parameters = *(vibration_detection_parameters *)args;
    while (true)
    {
        int64_t duration = detect_vibration(parameters.timer, parameters.detection_threshold_ms);
        if (duration >= 0)
        {
            Serial.println("Vibration detected (" + String(duration) + ")");
            if (xSemaphoreTake(*parameters.vibration_counter_sem, portMAX_DELAY) == pdTRUE)
            {
                *(parameters.vibration_counter)+=1;
                if(DEBUG){
                    Serial.println("Vibration counter: " + String(*(parameters.vibration_counter)));
                }
                xSemaphoreGive(*parameters.vibration_counter_sem);
            }
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

BaseType_t create_vibration_dection_task(TaskHandle_t *task_handle, vibration_detection_parameters *parameter)
{
    return xTaskCreatePinnedToCore(
        vibration_dection_task,
        "vibration detection",
        10000, /* Stack size in words */
        parameter,
        1, /* Priority of the task */
        task_handle,
        tskNO_AFFINITY);
}