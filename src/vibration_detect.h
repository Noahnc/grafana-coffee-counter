#ifndef VIBRATION_DETECT_INCLUDED
#define VIBRATION_DETECT_INCLUDED

#include "config.h"
#include <Arduino.h>

typedef struct {
    /// @brief Timer to use for time measuring.
    hw_timer_t *timer;
    /// @brief Minimum time a vibration must persist to be detected.
    int32_t detection_threshold_ms;
    /// @brief Semaphore that must be taken to update the counter.
    SemaphoreHandle_t *vibration_counter_sem;
    /// @brief Counter to increase when a vibration is detected.
    int64_t *vibration_counter;
} vibration_detection_parameters;

BaseType_t create_vibration_dection_task(TaskHandle_t *task_handle, vibration_detection_parameters *parameter);

#endif
