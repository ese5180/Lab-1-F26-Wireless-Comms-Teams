/*
 * Copyright (c) 2019 Manivannan Sadhasivam
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/drivers/lora.h>
#include <zephyr/drivers/gpio.h>
#include <errno.h>
#include <string.h>
#include <zephyr/sys/util.h>
#include <zephyr/kernel.h>

#define DEFAULT_RADIO_NODE DT_ALIAS(lora0)

BUILD_ASSERT(DT_NODE_HAS_STATUS_OKAY(DEFAULT_RADIO_NODE),
             "No default LoRa radio specified in DT");

#define MAX_DATA_LEN 255

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(lora_receive);

/* Explicit node label bindings for Nucleo-WL55JC discrete LEDs */
static const struct gpio_dt_spec leds[] = {
    GPIO_DT_SPEC_GET(DT_NODELABEL(blue_led_1), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(green_led_2), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(red_led_3), gpios)
};

static uint8_t active_led_idx = 0;

void lora_receive_cb(const struct device *dev, uint8_t *data, uint16_t size,
                     int16_t rssi, int8_t snr, void *user_data)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(user_data);
    const char team_id[] = "ese5180t10-";

    if (size != 12 || memcmp(data, team_id, 11) != 0) {
        return;
    }

    /* Turn OFF current active LED */
    if (gpio_is_ready_dt(&leds[active_led_idx])) {
        gpio_pin_set_dt(&leds[active_led_idx], 0);
    }

    /* Step to next LED (Blue -> Green -> Red) */
    active_led_idx = (active_led_idx + 1) % ARRAY_SIZE(leds);

    /* Turn ON new active LED */
    if (gpio_is_ready_dt(&leds[active_led_idx])) {
        gpio_pin_set_dt(&leds[active_led_idx], 1);
    }

    /* Safe null-termination for payload printing */
    char rx_buf[MAX_DATA_LEN + 1];
    uint16_t print_size = MIN(size, MAX_DATA_LEN);

    memcpy(rx_buf, data, print_size);
    rx_buf[print_size] = '\0';
    if (Payload == 10){
        LOG_INF("RX RSSI: %d dBm | SNR: %d dB | Payload: %s (Active LED: %d)",
            rssi, snr, rx_buf, active_led_idx);
    LOG_HEXDUMP_INF(data, size, "Raw Bytes");
    }
    
}

int main(void)
{
    const struct device *const lora_dev = DEVICE_DT_GET(DEFAULT_RADIO_NODE);
    struct lora_modem_config config = {0};
    int ret;

    if (!device_is_ready(lora_dev)) {
        LOG_ERR("%s Device not ready", lora_dev->name);
        return 0;
    }

    /* Configure all 3 discrete LEDs */
    for (size_t i = 0; i < ARRAY_SIZE(leds); i++) {
        if (gpio_is_ready_dt(&leds[i])) {
            gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_INACTIVE);
        }
    }

    /* Turn ON Blue LED at boot to show RX ready state */
    if (gpio_is_ready_dt(&leds[0])) {
        gpio_pin_set_dt(&leds[0], 1);
    }

    /* --- Physical Layer Matching with 433.92 MHz TX --- */
    config.frequency = 433920000;      /* 433.92 MHz */
    config.bandwidth = BW_500_KHZ;     /* 125 kHz */
    config.datarate = SF_7;           /* SF10 */
    config.preamble_len = 8;
    config.coding_rate = CR_4_5;
    config.iq_inverted = false;
    config.public_network = false;
    config.tx = false;

    ret = lora_config(lora_dev, &config);
    if (ret < 0) {
        LOG_ERR("LoRa config failed");
        return 0;
    }

    LOG_INF("Listening continuously on 433.92 MHz (SF10)...");

    /* Start asynchronous reception mode */
    ret = lora_recv_async(lora_dev, lora_receive_cb, NULL);
    if (ret < 0) {
        LOG_ERR("Failed to start async receive: %d", ret);
        return 0;
    }

    k_sleep(K_FOREVER);
    return 0;
}
