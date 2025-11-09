#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace arv_rs485_logger {

static const char *const TAG = "arv_rs485_logger";

// Helper to pretty hex without spaces if you need exact matching
static std::string hex_join(const std::vector<uint8_t> &v, const char *sep = ".") {
  char buf[4];
  std::string s;
  for (size_t i = 0; i < v.size(); i++) {
    snprintf(buf, sizeof(buf), "%02X", v[i]);
    if (i) s += sep;
    s += buf;
  }
  return s;
}

class ArvRs485Logger : public Component, public uart::UARTDevice {
 public:
  ArvRs485Logger() = default;

  // Tuning knobs
  uint32_t gap_us = 4000;          // inter-byte gap to break a burst
  size_t min_len_to_log = 4;       // ignore tiny splinters
  bool log_everything = false;     // set true to dump all bursts
  uint32_t learn_ms = 2000;        // time to learn frequent bursts
  uint32_t chatter_threshold = 40; // how many times within window to consider "chatter"
  bool highlight_sentinels = true; // mark bursts that end with FE/7E

  void setup() override {
    last_byte_us_ = micros();
    learn_until_ms_ = millis() + learn_ms;
    freq_window_start_ms_ = millis();
  }

  void loop() override {
    const uint32_t now_us = micros();

    // If we have an open burst and a big gap elapsed, flush it
    if (!buf_.empty() && (now_us - last_byte_us_) > gap_us) {
      handle_burst(buf_);
      buf_.clear();
    }

    // Drain UART
    while (this->available()) {
      uint8_t b;
      this->read_byte(&b);
      // New burst if big gap
      const uint32_t t = micros();
      if (!buf_.empty() && (t - last_byte_us_) > gap_us) {
        handle_burst(buf_);
        buf_.clear();
      }
      buf_.push_back(b);
      last_byte_us_ = t;
    }
  }

 protected:
  std::vector<uint8_t> buf_;
  uint32_t last_byte_us_{0};

  // Frequency learning
  std::unordered_map<std::string, uint16_t> freq_; // signature -> count in window
  uint32_t freq_window_start_ms_{0};
  uint32_t learn_until_ms_{0};

  bool is_likely_chatter(const std::vector<uint8_t> &burst) {
    // Short bursts dominated by the “segment pattern” set are usually display scans/status beacons.
    // Heuristic: values limited to this set and length <= 6 are likely chatter.
    static const uint8_t kSet[] = {
      0x00,0x06,0x18,0x1E,0x60,0x66,0x78,0x7E,0x80,0x86,0x98,0x9E,0xE0,0xE6,0xF8,0xFE
    };
    auto in_set = [](uint8_t x) {
      for (auto v : kSet) if (x == v) return true;
      return false;
    };
    size_t in = 0;
    for (uint8_t b : burst) if (in_set(b)) in++;
    if (burst.size() <= 6 && in == burst.size()) return true;
    return false;
  }

  void handle_burst(const std::vector<uint8_t> &burst) {
    if (burst.size() < min_len_to_log) return;

    // Rolling window for frequency counts (5s)
    const uint32_t now = millis();
    if (now - freq_window_start_ms_ > 5000) {
      freq_.clear();
      freq_window_start_ms_ = now;
    }

    const std::string sig = hex_join(burst, "");  // compact signature (no dots)
    auto &cnt = freq_[sig];
    cnt++;

    const bool ends_with_sentinel = !burst.empty() && (burst.back() == 0xFE || burst.back() == 0x7E);
    const bool chatterish = is_likely_chatter(burst);
    const bool learned = (now < learn_until_ms_);

    // Suppress very frequent patterns unless we are in "log everything" or learning phase
    if (!log_everything) {
      if (cnt >= chatter_threshold || chatterish) {
        if (learned) {
          ESP_LOGV(TAG, "[learn] mute soon %s (%u)", format_hex_pretty(burst).c_str(), cnt);
        }
        return;
      }
    }

    // Emphasize potentially interesting frames
    if (highlight_sentinels && ends_with_sentinel && burst.size() >= 6) {
      ESP_LOGI(TAG, "FRAME %s (%u)", format_hex_pretty(burst).c_str(), (unsigned)burst.size());
    } else {
      ESP_LOGD(TAG, "Burst %s (%u)", format_hex_pretty(burst).c_str(), (unsigned)burst.size());
    }
  }
};

}  // namespace arv_rs485_logger
}  // namespace esphome
