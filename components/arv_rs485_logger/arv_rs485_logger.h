#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace arv_rs485_logger {

static const char *const TAG = "arv_rs485_logger";

/**
 * Pure passive sniffer.
 * - Collects bytes continuously
 * - Splits bursts by inter-byte gap
 * - Always prints everything (no filters)
 * Use this to capture raw bus traffic for pattern analysis.
 */
class ArvRs485Logger : public Component, public uart::UARTDevice {
 public:
  ArvRs485Logger() = default;

  // keep the setters so YAML still compiles, but ignore them
  void set_min_gap_us(uint32_t v)   { min_gap_us_ = v; }
  void set_max_burst_len(size_t v)  { max_burst_len_ = v; }
  void set_min_length(size_t v)     { min_length_ = v; }
  void set_dedupe_ms(uint32_t)      {}
  void set_idle_filter(bool)        {}
  void set_idle_bytes(const std::vector<int>&) {}

  void setup() override {
    ESP_LOGI(TAG, "RAW sniffer active (gap=%u µs, max_len=%u)", 
             (unsigned)min_gap_us_, (unsigned)max_burst_len_);
  }

  void loop() override {
    const uint32_t now_us = micros();

    // new burst if idle gap exceeded
    if (!buf_.empty() && (now_us - last_byte_us_) > min_gap_us_) {
      flush_();
    }

    // read bytes
    while (this->available()) {
      uint8_t b;
      if (!this->read_byte(&b)) break;
      buf_.push_back(b);
      last_byte_us_ = micros();
      if (buf_.size() >= max_burst_len_) flush_();
    }
  }

 private:
  void flush_() {
    if (buf_.empty()) return;
    ESP_LOGD(TAG, "Burst %u bytes: %s", 
             (unsigned)buf_.size(), format_hex_pretty(buf_).c_str());
    buf_.clear();
  }

  std::vector<uint8_t> buf_;
  uint32_t last_byte_us_{0};
  uint32_t min_gap_us_{1200};   // ≈1.2 ms gap → new burst
  size_t   max_burst_len_{256};
  size_t   min_length_{1};
};

}  // namespace arv_rs485_logger
}  // namespace esphome
