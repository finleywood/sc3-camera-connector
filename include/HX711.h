#pragma once

#include <gpiod.h>
#include <unistd.h>
#include <stdexcept>

class HX711 {
public:
    HX711(const char* chip_path, unsigned int dout, unsigned int sck)
        : dout_offset(dout), sck_offset(sck)
    {
        chip = gpiod_chip_open(chip_path);
        if (!chip)
            throw std::runtime_error("Failed to open gpio chip");

        // -------- DOUT (input) --------
        gpiod_line_settings* dout_settings = gpiod_line_settings_new();
        gpiod_line_settings_set_direction(
            dout_settings, GPIOD_LINE_DIRECTION_INPUT);

        gpiod_line_config* dout_config = gpiod_line_config_new();
        gpiod_line_config_add_line_settings(
            dout_config, &dout_offset, 1, dout_settings);

        gpiod_request_config* dout_req_cfg = gpiod_request_config_new();
        gpiod_request_config_set_consumer(
            dout_req_cfg, "hx711_dout");

        dout_request = gpiod_chip_request_lines(
            chip, dout_req_cfg, dout_config);

        if (!dout_request)
            throw std::runtime_error("Failed to request DOUT");

        gpiod_line_settings_free(dout_settings);
        gpiod_line_config_free(dout_config);
        gpiod_request_config_free(dout_req_cfg);

        // -------- SCK (output) --------
        gpiod_line_settings* sck_settings = gpiod_line_settings_new();
        gpiod_line_settings_set_direction(
            sck_settings, GPIOD_LINE_DIRECTION_OUTPUT);
        gpiod_line_settings_set_output_value(
            sck_settings, GPIOD_LINE_VALUE_INACTIVE);

        gpiod_line_config* sck_config = gpiod_line_config_new();
        gpiod_line_config_add_line_settings(
            sck_config, &sck_offset, 1, sck_settings);

        gpiod_request_config* sck_req_cfg = gpiod_request_config_new();
        gpiod_request_config_set_consumer(
            sck_req_cfg, "hx711_sck");

        sck_request = gpiod_chip_request_lines(
            chip, sck_req_cfg, sck_config);

        if (!sck_request)
            throw std::runtime_error("Failed to request SCK");

        gpiod_line_settings_free(sck_settings);
        gpiod_line_config_free(sck_config);
        gpiod_request_config_free(sck_req_cfg);
    }

    ~HX711() {
        gpiod_line_request_release(dout_request);
        gpiod_line_request_release(sck_request);
        gpiod_chip_close(chip);
    }

    bool is_ready() const {
        return gpiod_line_request_get_value(dout_request, dout_offset) == 0;
    }

    long read() {
        while (!is_ready())
            usleep(10);

        long value = 0;
        for (int i = 0; i < 24; ++i) {
            clock_high();
            value <<= 1;

            int bit = gpiod_line_request_get_value(dout_request, dout_offset);
            if (bit)
                value++;

            clock_low();
        }

        // Gain = 128 (1 extra pulse)
        clock_high();
        clock_low();

        if (value & 0x800000)
            value |= ~0xFFFFFF; // sign extend 24-bit

        if(value < 0)
            value = 0;

        return value;
    }

    long read_with_timeout(unsigned int timeout_ms = 100) {
        unsigned int waited = 0;
        while (!is_ready()) {
            usleep(1000); // 1 ms
            waited++;
            if (waited >= timeout_ms) {
                return 0;
            }
        }

        long value = 0;
        for (int i = 0; i < 24; ++i) {
            clock_high();
            value <<= 1;
            if (gpiod_line_request_get_value(dout_request, dout_offset))
                value++;
            clock_low();
        }

        // Gain 128
        clock_high();
        clock_low();

        if (value & 0x800000)
            value |= ~0xFFFFFF; // sign extend

        if(value < 0)
            value = 0;

        return value;
    }

    long read_average(int samples = 5) {
        long sum = 0;
        for (int i = 0; i < samples; ++i)
            sum += read();
        return sum / samples;
    }

    long read_average_with_timeout(int samples = 5, unsigned int timeout_ms = 100) {
        long sum = 0;
        for (int i = 0; i < samples; ++i)
            sum += read_with_timeout(timeout_ms);
        return sum / samples;
    }

private:
    void clock_high() const {
        gpiod_line_request_set_value(
            sck_request, sck_offset, GPIOD_LINE_VALUE_ACTIVE);
        usleep(1);
    }

    void clock_low() const {
        gpiod_line_request_set_value(
            sck_request, sck_offset, GPIOD_LINE_VALUE_INACTIVE);
        usleep(1);
    }

    struct gpiod_chip* chip;
    struct gpiod_line_request* dout_request;
    struct gpiod_line_request* sck_request;
    unsigned int dout_offset;
    unsigned int sck_offset;
};