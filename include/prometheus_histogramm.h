#ifndef Prometheus_Histogramm_INCLUDED
#define Prometheus_Histogramm_INCLUDED

#include "config.h"
#include <Arduino.h>
#include <PrometheusArduino.h>

class Prometheus_Histogramm
{
private:
    TimeSeries **time_series_buckets;
    TimeSeries *time_series_count;
    TimeSeries *time_series_sum;
    int16_t buckets_start_value;
    int16_t buckets_value_increment;
    int16_t bucket_count;
    int16_t series_size;
    int64_t *bucket_le_values;
    int64_t *bucket_counters;
    int64_t sum = 0;
    int64_t count = 0;
    char *name;
    char *labels;
    bool initialized = false;
    SemaphoreHandle_t update_sem;

public:
    Prometheus_Histogramm(char *name, char *labels, int16_t series_size, int16_t buckets_start_value, int16_t buckets_value_increment, int16_t bucket_count);
    void init(WriteRequest &req);
    void AddValue(int16_t value);
    void Ingest(int64_t timestamp);
    void resetSamples();
};

#endif
