#pragma once
// Stand-in for the ESP-IDF NVS functions NvsStorage.cpp calls directly.
#include "FakeNvs.h"

typedef int esp_err_t;
#define ESP_OK 0

struct nvs_stats_t
{
    size_t used_entries;
    size_t free_entries;
    size_t total_entries;
    size_t namespace_count;
};

inline esp_err_t nvs_get_stats(const char *, nvs_stats_t *stats)
{
    stats->free_entries = FakeNvs::rawFreeEntries;
    return ESP_OK;
}

inline esp_err_t nvs_flash_erase()
{
    FakeNvs::store.clear();
    return ESP_OK;
}

inline esp_err_t nvs_flash_init() { return ESP_OK; }
