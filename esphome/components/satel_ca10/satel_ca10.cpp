#include "satel_ca10.h"
#include "esphome/core/log.h"

namespace esphome {
namespace satel_ca10 {

static const char *const TAG = "satel_ca10";

// A pause longer than this (us) between clock edges marks the start of a new frame
static const uint32_t FRAME_GAP_US = 2000;

void SatelCA10Hub::setup() {
  this->clk_pin_->setup();
  this->data_pin_->setup();
  this->isr_clk_pin_ = this->clk_pin_->to_isr();
  this->isr_data_pin_ = this->data_pin_->to_isr();
  this->clk_pin_->attach_interrupt(&SatelCA10Hub::gpio_intr, this, gpio::INTERRUPT_ANY_EDGE);
}

void IRAM_ATTR SatelCA10Hub::gpio_intr(SatelCA10Hub *arg) {
  uint32_t now = micros();

  // New message starts if the gap since the last edge is long enough (but not for the very first edge)
  if (arg->last_interrupt_time_ != 0 && (now - arg->last_interrupt_time_) > FRAME_GAP_US) {
    arg->bit_count_ = 0;
    arg->data_ready_ = false;
  }
  arg->last_interrupt_time_ = now;

  arg->prev_clk_state_ = arg->clk_state_;
  arg->clk_state_ = arg->isr_clk_pin_.digital_read();

  // Clock did not toggle - lost sync, wait for next frame gap
  if (arg->prev_clk_state_ == arg->clk_state_ && arg->bit_count_ > 0) {
    arg->bit_count_ = FRAME_SIZE + 10;
    arg->data_ready_ = false;
    return;
  }

  if (arg->bit_count_ < FRAME_SIZE) {
    // let the DATA line settle after the clock edge (pull-up)
    delayMicroseconds(100);
    arg->data_result_[arg->bit_count_] = arg->isr_data_pin_.digital_read();
  }

  arg->bit_count_ = arg->bit_count_ + 1;
  if (arg->bit_count_ == FRAME_SIZE)
    arg->data_ready_ = true;
}

void SatelCA10Hub::loop() {
  if (!this->data_ready_)
    return;

  bool frame[FRAME_SIZE];
  for (uint8_t i = 0; i < FRAME_SIZE; i++)
    frame[i] = this->data_result_[i];
  this->data_ready_ = false;

  for (auto &s : this->sensors_) {
    bool state = frame[s.first];
    if (this->first_frame_ || state != this->published_[s.first])
      s.second->publish_state(state);
  }
  for (uint8_t i = 0; i < FRAME_SIZE; i++)
    this->published_[i] = frame[i];
  this->first_frame_ = false;
}

void SatelCA10Hub::dump_config() {
  ESP_LOGCONFIG(TAG, "Satel CA10:");
  LOG_PIN("  CLK Pin: ", this->clk_pin_);
  LOG_PIN("  DATA Pin: ", this->data_pin_);
  ESP_LOGCONFIG(TAG, "  Registered sensors: %u", (unsigned) this->sensors_.size());
}

}  // namespace satel_ca10
}  // namespace esphome
