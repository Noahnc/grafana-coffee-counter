#ifndef VIBRATION_DETECT_INCLUDED
#define VIBRATION_DETECT_INCLUDED

#include "config.h"
#include <Arduino.h>
#include <prometheus_histogramm.h>

typedef struct
{
    /// @brief Timer to use for time measuring.
    hw_timer_t *timer;
    /// @brief Minimum vibration detection time in ms for the differnet coffee types.
    int32_t vibration_detection_threshold_ms;
    /// @brief Semaphore that must be taken to update the counter.
    SemaphoreHandle_t *vibration_counter_sem;
    /// @brief Counter to increase for the different coffee types.
    Prometheus_Histogramm *coffees_consumed;

} vibration_detection_parameters;

BaseType_t create_vibration_dection_task(TaskHandle_t *task_handle, vibration_detection_parameters *parameter);

#endif
