#ifndef VIBRATION_INCLUDED
#define VIBRATION_INCLUDED

#include "config.h"
#include <Arduino.h>
#include <prometheus_histogram.h>

class Vibration
{
public:
    Vibration(hw_timer_t *timer, int32_t vibration_detection_threshold_ms, Prometheus_Histogramm *coffees_consumed);
    ~Vibration();
    void beginAsync();

private:
    TaskHandle_t vibration_detection_task;
    hw_timer_t *timer;
    int32_t vibration_detection_threshold_ms;
    Prometheus_Histogramm *coffees_consumed;

    static void vibration_dection_task(void *args);
    static int64_t detect_vibration(hw_timer_t *timer);
};

#endif
