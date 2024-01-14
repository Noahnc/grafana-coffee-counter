#ifndef VIBRATION_DETECT_INCLUDED
#define VIBRATION_DETECT_INCLUDED

#include "config.h"
#include <Arduino.h>

typedef struct
{
    /// @brief Timer to use for time measuring.
    hw_timer_t *timer;
    /// @brief Minimum vibration detection time in ms for the differnet coffee types.
    int32_t detection_threshold_ms_small_coffee;
    int32_t detection_threshold_ms_medium_coffee;
    int32_t detection_threshold_ms_large_coffee;
    /// @brief Semaphore that must be taken to update the counter.
    SemaphoreHandle_t *vibration_counter_sem;
    /// @brief Counter to increase for the different coffee types.
    int64_t *vibration_counter_small_coffee;
    int64_t *vibration_counter_medium_coffee;
    int64_t *vibration_counter_large_coffee;
} vibration_detection_parameters;

BaseType_t create_vibration_dection_task(TaskHandle_t *task_handle, vibration_detection_parameters *parameter);

#endif
