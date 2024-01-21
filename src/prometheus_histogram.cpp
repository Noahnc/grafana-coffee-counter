#include "prometheus_histogram.h"
#include "config.h"

Prometheus_Histogram::Prometheus_Histogram(char *name, const char *labels, int16_t series_size, int16_t buckets_start_value, int16_t buckets_value_increment, int16_t bucket_count)
{
    this->name = name;
    this->labels = labels;
    this->series_size = series_size;
    this->buckets_start_value = buckets_start_value;
    this->buckets_value_increment = buckets_value_increment;

    char time_series_count_name[strlen(name) + 6];
    char time_series_sum_name[strlen(name) + 4];
    strcpy(time_series_count_name, name);
    strcpy(time_series_sum_name, name);
    strcat(time_series_count_name, "_count");
    strcat(time_series_sum_name, "_sum");

    this->time_series_count = new TimeSeries(series_size, time_series_count_name, labels);
    this->time_series_sum = new TimeSeries(series_size, time_series_sum_name, labels);

    // We need one more bucket for the "+Inf" bucket
    this->bucket_count = bucket_count + 1;

    this->bucket_le_values = new int64_t[bucket_count];
    this->bucket_counters = new int64_t[bucket_count];
    time_series_buckets = new TimeSeries *[bucket_count];
    for (int i = 0; i < bucket_count; i++)
    {
        time_series_buckets[i] = nullptr; // Initialize with nullptr
    }

    update_sem = xSemaphoreCreateBinary();
    xSemaphoreGive(update_sem);
}

void Prometheus_Histogram::init(WriteRequest &req)
{
    for (int i = 0; i < bucket_count; i++)
    {
        this->bucket_le_values[i] = buckets_start_value + i * buckets_value_increment;
        this->bucket_counters[i] = 0;

        // Using std::string for safer string operations
        std::string bucket_labels = labels;

        // Find the position of the closing brace
        size_t closing_brace_pos = bucket_labels.find_last_of('}');
        if (closing_brace_pos != std::string::npos)
        {
            // Insert the new label before the closing brace
            if (i == bucket_count - 1)
            {
                // The last bucket is labeled with "+Inf"
                std::string new_label = ",le=\"+Inf\"";
                bucket_labels.insert(closing_brace_pos, new_label);
            }
            else
            {
                // All other buckets are labeled with the upper bound of the bucket (exclusive
                std::string new_label = ",le=\"" + std::to_string(this->bucket_le_values[i]) + "\"";
                bucket_labels.insert(closing_brace_pos, new_label);
            }
        }

        if (DEBUG)
        {
            String bucket_name = "le=" + String(this->bucket_le_values[i]);
            if (i == bucket_count - 1)
            {
                bucket_name = "le=+Inf";
            }
            Serial.println("Initializing bucket " + String(i) + " with " + bucket_name + " and labels " + String(bucket_labels.c_str()));
        }

        // Initialize the TimeSeries object for the current bucket
        time_series_buckets[i] = new TimeSeries(series_size, name, bucket_labels.c_str());
        req.addTimeSeries(*time_series_buckets[i]);
    }

    req.addTimeSeries(*time_series_count);
    req.addTimeSeries(*time_series_sum);
}

void Prometheus_Histogram::AddValue(int16_t value)
{
    if (DEBUG)
    {
        Serial.println("Adding value " + String(value) + " to histogram " + String(this->name));
    }
    if (xSemaphoreTake(update_sem, portMAX_DELAY) == pdTRUE)
    {
        bool found = false;
        // Increment all bucket counters for which the value is smaller than the bucket value
        for (int i = 0; i < bucket_count - 1; i++)
        {
            if (value <= bucket_le_values[i])
            {
                bucket_counters[i] += 1;
                if (DEBUG)
                    Serial.println("Incrementing counter of bucket le=" + String(bucket_le_values[i]) + " with new count " + String(bucket_counters[i]) + " for histogram " + String(this->name));
                found = true;
            }
        }
        if (!found)
        {
            // Increment the counter for the last bucket (which is labeled with "+Inf")
            Serial.println("Value " + String(value) + " is larger than all buckets of histogram " + String(this->name) + ", incrementing counter of bucket +Inf");
            bucket_counters[bucket_count - 1] += 1;
        }
        sum += value;
        count += 1;
        xSemaphoreGive(update_sem);
    }
}

void Prometheus_Histogram::Ingest(int64_t timestamp)
{
    if (DEBUG)
    {
        Serial.println("Ingesting histogram " + String(this->name));
        Serial.println("Histogram " + String(this->name) + " has count " + String(count) + " and sum " + String(sum) + " at " + String(timestamp));
    }

    if (xSemaphoreTake(update_sem, portMAX_DELAY) == pdTRUE)
    {
        time_series_sum->addSample(timestamp, sum);
        time_series_count->addSample(timestamp, count);
        for (int i = 0; i < bucket_count; i++)
        {
            if (DEBUG)
            {
                String bucket_name = "le=" + String(this->bucket_le_values[i]);
                if (i == bucket_count - 1)
                {
                    bucket_name = "le=+Inf";
                }
                Serial.println("Histogram " + String(this->name) + " bucket " + i + " " + bucket_name + " has count " + String(bucket_counters[i]));
            }
            time_series_buckets[i]->addSample(timestamp, bucket_counters[i]);
        }
        xSemaphoreGive(update_sem);
    }
}

void Prometheus_Histogram::resetSamples()
{
    if (xSemaphoreTake(update_sem, portMAX_DELAY) == pdTRUE)
    {
        for (int i = 0; i < bucket_count; i++)
        {
            time_series_buckets[i]->resetSamples();
        }
        time_series_sum->resetSamples();
        time_series_count->resetSamples();
        xSemaphoreGive(update_sem);
    }
}
