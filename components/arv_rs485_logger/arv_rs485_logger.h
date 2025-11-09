#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <algorithm>
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace arv_rs485_logger {

static const char *const TAG = "arv_rs485_logger";

class ArvRs485Logger : public Component, public uart::UARTDevice {
 public:
  ArvRs485Logger() = default;

  // ---- Setters (from __init__.py) ----
  void set_min_gap_us(uint32_t v)   { min_gap_us_ = v; }
  void set_max_burst_len(size_t v)  { max_burst_len_ = v; }
  void set_min_length(size_t v)     { min_length_ = v; }          // kept for API compat; not the main filter
  void set_dedupe_ms(uint32_t v)    { dedupe_ms_ = v; }
  void set_idle_filter(bool v)      { idle_filter_ = v; }
  void set_idle_bytes(const std::vector<int> &v) {
    idle_bytes_.clear(); idle_bytes_.reserve(v.size());
    for (int x : v) idle_bytes_.push_back(uint8_t(x & 0xFF));
    std::sort(idle_bytes_.begin(), idle_bytes_.end());
    idle_bytes_.erase(std::unique(idle_bytes_.begin(), idle_bytes_.end()), idle_bytes_.end());
  }

  void setup() override {
    ESP_LOGI(TAG,
      "Sniffer: gap=%u us, max_len=%u, dedupe=%u ms, idle_filter=%s (idle_set=%u)",
      (unsigned)min_gap_us_, (unsigned)max_burst_len_, (unsigned)dedupe_ms_,
      idle_filter_ ? "on" : "off", (unsigned)idle_bytes_.size());
  }

  void loop() override {
    const uint32_t now_us = micros();
    const uint32_t now_ms = millis();

    // Gap-based flush
    if (!burst_.empty() && (now_us - last_byte_us_) > min_gap_us_) flush_();

    // Safety: force flush every 50 ms in case stream never idles
    if (!burst_.empty() && (now_ms - last_force_flush_ms_) > 50) {
      flush_();
      last_force_flush_ms_ = now_ms;
    }

    // Drain UART
    while (this->available()) {
      uint8_t b; if (!this->read_byte(&b)) break;
      if (burst_.size() >= max_burst_len_) flush_();
      burst_.push_back(b);
      last_byte_us_ = micros();
    }
  }

 private:
  // ---------- Core filter logic (agnostic, minimal assumptions) ----------
  // We DROP only bursts that are both:
  //  (a) very short (<= 4 bytes), AND
  //  (b) composed entirely of glyph/idle bytes (7-segment chatter).
  // We KEEP everything else, plus ALWAYS keep bursts ending with 0x7E/0xFE (common frame tails).
  bool is_pure_idle_glyph_(const std::vector<uint8_t>& v) const {
    if (!idle_filter_) return false;
    if (v.size() > 4) return false;                 // only kill tiny bursts
    if (idle_bytes_.empty()) return false;
    for (auto b : v)
      if (!std::binary_search(idle_bytes_.begin(), idle_bytes_.end(), b))
        return false;                               // has a non-idle byte -> not pure glyph
    return true;                                    // all bytes are idle-set & tiny
  }

  bool passes_dedupe_(const std::vector<uint8_t>& v) {
    if (!dedupe_ms_) return true;
    const uint32_t now = millis();
    if (v == last_printed_ && (now - last_printed_ms_) < dedupe_ms_) return false;
    last_printed_ = v; last_printed_ms_ = now; return true;
  }

  void flush_() {
    if (burst_.empty()) return;

    bool keep = true;

    // Always keep if looks like framed tail (very lightweight heuristic)
    const uint8_t tail = burst_.back();
    const bool has_frame_tail = (tail == 0x7E || tail == 0xFE);

    if (!has_frame_tail) {
      // Drop only the tiny pure-glyph chatter
      if (is_pure_idle_glyph_(burst_)) keep = false;
      // Also respect optional minimum length if user set it > 1
      if (keep && min_length_ > 1 && burst_.size() < min_length_) keep = false;
    }

    if (keep && !passes_dedupe_(burst_)) keep = false;

    if (keep) {
      ESP_LOGD(TAG, "Burst %u bytes: %s",
               (unsigned)burst_.size(), format_hex_pretty(burst_).c_str());
    }

    burst_.clear();
  }

  // ---------- State ----------
  std::vector<uint8_t> burst_;
  std::vector<uint8_t> last_printed_;
  std::vector<uint8_t> idle_bytes_;
  uint32_t last_byte_us_{0};
  uint32_t last_force_flush_ms_{0};
  uint32_t last_printed_ms_{0};

  // ---------- Tunables (overridden via setters from YAML) ----------
  uint32_t min_gap_us_{1200};
  size_t   max_burst_len_{512};
  size_t   min_length_{1};
  uint32_t dedupe_ms_{0};
  bool     idle_filter_{true};   // ON by default, but only kills tiny pure-glyph bursts
};

}  // namespace arv_rs485_logger
}  // namespace esphome