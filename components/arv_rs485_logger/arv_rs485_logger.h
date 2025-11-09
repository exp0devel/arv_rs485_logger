#pragma once

#include <vector>
#include <string>
#include <cstdio>
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace arv_rs485_logger {

// unified tag for logs
static const char *const TAG = "arv_rs485_logger";

/**
 * A UART burst sniffer:
 * - collects bytes into a buffer
 * - if inter-byte gap > min_gap_us, flushes the buffer as one "burst"
 * - prints HEX and ASCII-ish view for quick eyeballing
 */
class ArvRs485Logger : public Component, public uart::UARTDevice {
 public:
  ArvRs485Logger() = default;

  void set_min_gap_us(uint32_t us)   { this->min_gap_us_ = us; }
  void set_max_burst_len(size_t len) { this->max_burst_len_ = len; }

  void setup() override {
    ESP_LOGI(TAG, "Sniffer ready. min_gap_us=%u, max_burst_len=%u",
             (unsigned) this->min_gap_us_, (unsigned) this->max_burst_len_);
  }

  void loop() override {
    const uint32_t now = micros();

    // If we have data accumulated and we've been idle for long enough, flush it.
    if (!buf_.empty() && elapsed_since_last(now) > this->min_gap_us_) {
      flush_burst_();
    }

    // Consume all available bytes
    while (this->available()) {
      uint8_t b;
      if (!this->read_byte(&b)) break;

      if (buf_.size() < this->max_burst_len_) {
        buf_.push_back(b);
      } else {
        // Hard flush if someone sent a very long block without gaps
        flush_burst_();
        buf_.push_back(b);
      }
      last_byte_us_ = now_safe_();
    }
  }

 protected:
  // --- helpers ---

  // elapsed microseconds since last byte, using the current micros()
  inline uint32_t elapsed_since_last(uint32_t now) const {
    return now - this->last_byte_us_;  // micros() is unsigned and wraps safely
  }

  inline uint32_t now_safe_() const { return micros(); }

  static std::string ascii_hint_(const std::vector<uint8_t> &v) {
    // Build a printable ASCII string, dots for non-printables
    std::string s;
    s.reserve(v.size());
    for (auto b : v) {
      if (b >= 32 && b <= 126) s.push_back(char(b));
      else s.push_back('.');
    }
    return s;
  }

  void flush_burst_() {
    if (buf_.empty()) return;

    // Pretty hex dump
    std::string hex = format_hex_pretty(buf_);
    // ASCII hint (not perfect but helpful)
    std::string asc = ascii_hint_(buf_);

    // One line at DEBUG, raw bytes at VERY_VERBOSE
    ESP_LOGD(TAG, "Burst %u bytes: %s", (unsigned) buf_.size(), hex.c_str());
    ESP_LOGV(TAG, "ASCII: %s", asc.c_str());

    buf_.clear();
  }

  // --- state ---
  std::vector<uint8_t> buf_;
  uint32_t last_byte_us_ = 0;
  uint32_t min_gap_us_   = 5000;  // default 5 ms
  size_t   max_burst_len_ = 256;  // default safety cap
};

}  // namespace arv_rs485_logger
}  // namespace esphome
