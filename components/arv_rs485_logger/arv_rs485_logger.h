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

  // ==== Setters used from __init__.py ====
  void set_min_gap_us(uint32_t v)   { min_gap_us_ = v; }
  void set_max_burst_len(size_t v)  { max_burst_len_ = v; }
  void set_min_length(size_t v)     { min_length_ = v; }
  void set_dedupe_ms(uint32_t v)    { dedupe_ms_ = v; }
  void set_idle_filter(bool v)      { idle_filter_ = v; }
  void set_idle_bytes(const std::vector<int> &v) {
    idle_bytes_.clear();
    idle_bytes_.reserve(v.size());
    for (int x : v) idle_bytes_.push_back(static_cast<uint8_t>(x & 0xFF));
    std::sort(idle_bytes_.begin(), idle_bytes_.end());
    idle_bytes_.erase(std::unique(idle_bytes_.begin(), idle_bytes_.end()), idle_bytes_.end());
  }

  void setup() override {
    ESP_LOGI(TAG,
      "Sniffer ready: gap=%u us, max_len=%u, min_len=%u, dedupe=%u ms, idle_filter=%s, idle_set=%u bytes",
      (unsigned) min_gap_us_, (unsigned) max_burst_len_, (unsigned) min_length_,
      (unsigned) dedupe_ms_, idle_filter_ ? "on" : "off", (unsigned) idle_bytes_.size());
  }

  void loop() override {
    const uint32_t now_us = micros();
    const uint32_t now_ms = millis();

    // Gap-based flush
    if (!burst_.empty() && (now_us - last_byte_us_) > min_gap_us_) {
      flush_();
    }

    // Safety: force flush every 50 ms in case stream never idles
    if (!burst_.empty() && (now_ms - last_force_flush_ms_) > force_flush_ms_) {
      flush_();
      last_force_flush_ms_ = now_ms;
    }

    // Drain UART
    while (this->available()) {
      uint8_t b;
      if (!this->read_byte(&b)) break;

      if (burst_.size() >= max_burst_len_) {
        flush_(); // avoid unbounded growth
      }
      burst_.push_back(b);
      last_byte_us_ = micros();
    }
  }

 private:
  // ---- helpers ----
  static bool is_printable(uint8_t b) { return b >= 32 && b <= 126; }

  bool looks_like_frame_(const std::vector<uint8_t>& v) const {
    // AUX frames usually end with FE/7E and are >=10 bytes
    if (v.size() < 10) return false;
    uint8_t last = v.back();
    if (!(last == 0xFE || last == 0x7E)) return false;

    // reject bursts composed only of idle/poll bytes
    if (idle_filter_ && v.size() <= 64 && !idle_bytes_.empty()) {
      bool all_idle = true;
      for (auto b : v) {
        if (!std::binary_search(idle_bytes_.begin(), idle_bytes_.end(), b)) {
          all_idle = false;
          break;
        }
      }
      if (all_idle) return false;
    }

    // require some byte variety (simple entropy test)
    uint8_t hist[256] = {0};
    int uniq = 0;
    for (uint8_t b : v) if (!hist[b]++) uniq++;
    if (uniq < std::min<int>(6, static_cast<int>(v.size() / 2))) return false;

    return true;
  }

  static std::string ascii_hint_(const std::vector<uint8_t> &v) {
    std::string s; s.reserve(v.size());
    for (auto b : v) s.push_back(is_printable(b) ? char(b) : '.');
    return s;
  }

  // ---- filters ----
  bool pass_min_length_(const std::vector<uint8_t> &v) const {
    return v.size() >= min_length_;
  }

  bool pass_dedupe_(const std::vector<uint8_t> &v) {
    const uint32_t now = millis();
    if (v == last_printed_ && (now - last_printed_ms_) < dedupe_ms_)
      return false;
    last_printed_ = v;
    last_printed_ms_ = now;
    return true;
  }

  bool pass_idle_set_(const std::vector<uint8_t> &v) const {
    if (!idle_filter_) return true;
    if (v.size() > 64) return true; // longer bursts are never idle chatter
    if (idle_bytes_.empty()) return true;
    for (auto b : v)
      if (!std::binary_search(idle_bytes_.begin(), idle_bytes_.end(), b))
        return true;
    return false;
  }

  // ---- main flush with heuristics ----
  void flush_() {
    if (burst_.empty()) return;

    bool keep = true;
    if (keep) keep = pass_min_length_(burst_);
    if (keep) keep = pass_idle_set_(burst_);
    if (keep) keep = pass_dedupe_(burst_);
    if (keep) keep = looks_like_frame_(burst_);  // <- new heuristics

    if (keep) {
      bool novel = false;
      if (burst_.size() <= 16) {
        if (std::find(seen_short_.begin(), seen_short_.end(), burst_) == seen_short_.end()) {
          novel = true;
          seen_short_.push_back(burst_);
          if (seen_short_.size() > 50) seen_short_.erase(seen_short_.begin());
        }
      }

      std::string hex = format_hex_pretty(burst_);
      ESP_LOGD(TAG, "%sBurst %u bytes: %s",
               novel ? "[NEW] " : "",
               (unsigned)burst_.size(),
               hex.c_str());
      ESP_LOGV(TAG, "ASCII: %s", ascii_hint_(burst_).c_str());
    }
    burst_.clear();
  }

  // ---- state ----
  std::vector<uint8_t> burst_;
  std::vector<uint8_t> last_printed_;
  std::vector<uint8_t> idle_bytes_;
  std::vector<std::vector<uint8_t>> seen_short_;
  uint32_t last_byte_us_{0};
  uint32_t last_force_flush_ms_{0};
  uint32_t last_printed_ms_{0};

  // ---- tunables (defaults; overridden by setters) ----
  uint32_t min_gap_us_{1200};
  size_t   max_burst_len_{256};
  size_t   min_length_{1};
  uint32_t dedupe_ms_{0};
  bool     idle_filter_{false};
  static constexpr uint32_t force_flush_ms_{50};
};

}  // namespace arv_rs485_logger
}  // namespace esphome
