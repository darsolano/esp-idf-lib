/**
 * @file hx711.c
 *
 * ESP-IDF driver for HX711 24-bit ADC for weigh scales
 *
 * Copyright (C) 2019 Ruslan V. Uss <unclerus@gmail.com>
 *
 * BSD Licensed as described in the file LICENSE
 */
#include "hx711.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/time.h>

#define CHECK(x) do { esp_err_t __; if ((__ = x) != ESP_OK) return __; } while (0)
#define CHECK_ARG(VAL) do { if (!(VAL)) return ESP_ERR_INVALID_ARG; } while (0)

static inline uint32_t get_time_ms()
{
    struct timeval time;
    gettimeofday(&time, 0);
    return time.tv_sec * 1000 + time.tv_usec / 1000;
}

static uint32_t read_raw(gpio_num_t dout, gpio_num_t pd_sck, hx711_gain_t gain)
{
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    portENTER_CRITICAL(&mux);

    // read data
    uint32_t data = 0;
    for (size_t i = 0; i < 24; i++)
    {
        gpio_set_level(pd_sck, 1);
        ets_delay_us(1);
        data |= gpio_get_level(dout) << (23 - i);
        gpio_set_level(pd_sck, 0);
        ets_delay_us(1);
    }

    // config gain + channel for next read
    for (size_t i = 0; i <= gain; i++)
    {
        gpio_set_level(pd_sck, 1);
        ets_delay_us(1);
        gpio_set_level(pd_sck, 0);
        ets_delay_us(1);
    }

    portEXIT_CRITICAL(&mux);

    return data;
}

///////////////////////////////////////////////////////////////////////////////

esp_err_t hx711_init(hx711_t *dev)
{
    CHECK_ARG(dev);

    CHECK(gpio_set_direction(dev->dout, GPIO_MODE_INPUT));
    CHECK(gpio_set_direction(dev->pd_sck, GPIO_MODE_OUTPUT));

    CHECK(hx711_power_down(dev, false));

    return hx711_set_gain(dev, dev->gain);
}

esp_err_t hx711_power_down(hx711_t *dev, bool down)
{
    CHECK_ARG(dev);

    CHECK(gpio_set_level(dev->pd_sck, down));
    vTaskDelay(1);

    return ESP_OK;
}

esp_err_t hx711_set_gain(hx711_t *dev, hx711_gain_t gain)
{
    CHECK_ARG(dev);
    CHECK_ARG(gain <= HX711_GAIN_A_64);

    CHECK(hx711_wait(dev, 200)); // 200 ms timeout

    read_raw(dev->dout, dev->pd_sck, gain);
    dev->gain = gain;

    return ESP_OK;
}

esp_err_t hx711_is_ready(hx711_t *dev, bool *ready)
{
    CHECK_ARG(dev);
    CHECK_ARG(ready);

    *ready = !gpio_get_level(dev->dout);

    return ESP_OK;
}

esp_err_t hx711_wait(hx711_t *dev, size_t timeout_ms)
{
    uint32_t started = get_time_ms();
    while (get_time_ms() - started < timeout_ms)
    {
        if (!gpio_get_level(dev->dout))
            return ESP_OK;
        vTaskDelay(1);
    }

    return ESP_ERR_TIMEOUT;
}

esp_err_t hx711_read_data(hx711_t *dev, int32_t *data)
{
    CHECK_ARG(dev);
    CHECK_ARG(data);

    uint32_t raw = read_raw(dev->dout, dev->pd_sck, dev->gain);
    if (raw & 0x800000)
        raw |= 0xff000000;
    *data = (int32_t)raw;

    return ESP_OK;
}
