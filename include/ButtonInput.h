#pragma once

#include <stdint.h>

enum class ButtonEvent { None, Armed, Pressed, Released };

struct ButtonState {
  explicit ButtonState(uint8_t gpio) : pin(gpio) {}
  uint8_t pin;
  bool rawHigh = true;
  bool stableHigh = true;
  bool longPressHandled = false;
  bool wakeOnlyUntilRelease = false;
  uint32_t rawChangedAt = 0;
  uint32_t pressedAt = 0;
  bool armed = false;
};

// Pure input filter: no Arduino, network, display, or light-control side effects.
inline ButtonEvent updateButtonInput(ButtonState& button, bool rawHigh,
                                     uint32_t now, uint32_t debounceMs,
                                     uint32_t armMs) {
  if (!button.armed) {
    if (!rawHigh) {
      button.rawChangedAt = now;
    } else if (now - button.rawChangedAt >= armMs) {
      button.armed = true;
      button.rawHigh = button.stableHigh = true;
      return ButtonEvent::Armed;
    }
    return ButtonEvent::None;
  }
  if (rawHigh != button.rawHigh) {
    button.rawHigh = rawHigh;
    button.rawChangedAt = now;
  }
  if (rawHigh == button.stableHigh ||
      now - button.rawChangedAt < debounceMs) {
    return ButtonEvent::None;
  }
  button.stableHigh = rawHigh;
  if (!rawHigh) {
    button.pressedAt = now;
    button.longPressHandled = false;
    return ButtonEvent::Pressed;
  }
  return ButtonEvent::Released;
}

enum class BootGesture { None, SingleTap, DoubleTap, HoldStart, HoldEnd };

struct BootGestureState {
  bool tapPending = false;
  bool secondPress = false;
  bool holding = false;
  uint32_t firstReleasedAt = 0;
};

// Single taps are delayed until the double-tap window closes. A second press
// reserves the pending tap until its release/hold, so no toggle leaks through.
inline BootGesture updateBootGesture(BootGestureState& state, ButtonEvent event,
                                     const ButtonState& button, bool rawHigh,
                                     uint32_t now, uint32_t doubleTapMs = 350,
                                     uint32_t holdMs = 500) {
  if (event == ButtonEvent::Pressed) {
    if (state.tapPending && now - state.firstReleasedAt < doubleTapMs) {
      state.secondPress = true;
    } else if (state.tapPending) {
      state.tapPending = false;
      state.secondPress = false;
      return BootGesture::SingleTap;
    }
  }

  if (!button.stableHigh && !rawHigh && !state.holding &&
      now - button.pressedAt >= holdMs) {
    state.tapPending = false;
    state.secondPress = false;
    state.holding = true;
    return BootGesture::HoldStart;
  }

  if (event == ButtonEvent::Released) {
    if (state.holding || now - button.pressedAt >= holdMs) {
      const bool wasHolding = state.holding;
      state = BootGestureState{};
      return wasHolding ? BootGesture::HoldEnd : BootGesture::None;
    }
    if (state.secondPress) {
      state = BootGestureState{};
      return BootGesture::DoubleTap;
    }
    state.tapPending = true;
    state.firstReleasedAt = now;
  }

  if (state.tapPending && !state.secondPress && button.stableHigh && rawHigh &&
      now - state.firstReleasedAt >= doubleTapMs) {
    state.tapPending = false;
    return BootGesture::SingleTap;
  }
  return BootGesture::None;
}
