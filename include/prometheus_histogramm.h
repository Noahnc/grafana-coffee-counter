#ifndef Prometheus_Histogramm_INCLUDED
#define Prometheus_Histogramm_INCLUDED

#include "config.h"
#include <Arduino.h>
#include <PrometheusArduino.h>

class Prometheus_Histogramm
{
private:
    TimeSeries **time_series_buckets;
    TimeSeries time_series_count;
    TimeSeries time_series_sum;
    int16_t bucket_start;
    int16_t bucket_increment;
    int16_t bucket_count;
    int16_t series_size;
    int16_t *bucket_values;
    int16_t *bucket_counters;
    int64_t sum;
    int16_t count;
    char *name;
    char *labels;

public:
    Prometheus_Histogramm(char *name, char *labels, int16_t series_size, int16_t bucket_start, int16_t bucket_increment, int16_t bucket_count);
    void init(WriteRequest *req);
    void AddValue(int16_t value);
    void Ingest(int64_t timestamp);
    void resetSamples();
    void associateWriteRequest(WriteRequest *req);
};

#endif
