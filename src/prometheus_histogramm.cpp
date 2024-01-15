#include "prometheus_histogramm.h"

Prometheus_Histogramm::Prometheus_Histogramm(char *name, char *labels, int16_t series_size, int16_t bucket_start, int16_t bucket_increment, int16_t bucket_count)
    : time_series_count(series_size, name, labels),
      time_series_sum(series_size, name, labels)
{
    this->name = name;
    this->labels = labels;
    this->series_size = series_size;
    this->bucket_start = bucket_start;
    this->bucket_increment = bucket_increment;
    this->bucket_count = bucket_count;
    this->bucket_values = new int16_t[bucket_count];
    this->bucket_counters = new int16_t[bucket_count];
    time_series_buckets = new TimeSeries *[bucket_count];
    for (int i = 0; i < bucket_count; i++)
    {
        time_series_buckets[i] = nullptr; // Initialize with nullptr
    }
}

void Prometheus_Histogramm::init()
{
    for (int i = 0; i < bucket_count; i++)
    {
        this->bucket_values[i] = bucket_start + i * bucket_increment;
        char bucket_labels[strlen(labels) + 20];
        strcpy(bucket_labels, labels);
        char bucket_label[20];
        sprintf(bucket_label, ",le=\"%d\"", this->bucket_values[i]);
        strcat(bucket_labels, bucket_label);

        // Initialize the TimeSeries object for the current bucket
        time_series_buckets[i] = new TimeSeries(series_size, name, bucket_labels);
    }
    char time_series_count_name[strlen(name) + 6];
    char time_series_sum_name[strlen(name) + 4];
    strcpy(time_series_count_name, name);
    strcpy(time_series_sum_name, name);
    strcat(time_series_count_name, "_count");
    strcat(time_series_sum_name, "_sum");
    time_series_count = TimeSeries(series_size, time_series_count_name, labels);
    time_series_sum = TimeSeries(series_size, time_series_sum_name, labels);
}

void Prometheus_Histogramm::AddValue(int16_t value)
{
    for (int i = 0; i < bucket_count; i++)
    {
        if (value <= bucket_values[i])
        {
            bucket_counters[i] += 1;
            break;
        }
    }
    sum += value;
    count += 1;
}

void Prometheus_Histogramm::Ingest(int64_t timestamp)
{
    for (int i = 0; i < bucket_count; i++)
    {
        time_series_buckets[i]->addSample(timestamp, bucket_counters[i]);
    }
    time_series_sum.addSample(timestamp, sum);
    time_series_count.addSample(timestamp, sum);
}
