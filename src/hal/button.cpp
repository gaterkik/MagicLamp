#include "button.h"

const char* buttonEventToString(ButtonEvent event) {
    switch (event) {
        case BUTTON_EVENT_NONE:
            return "BUTTON_EVENT_NONE";
        case BUTTON_EVENT_SINGLE_CLICK:
            return "BUTTON_EVENT_SINGLE_CLICK";
        case BUTTON_EVENT_DOUBLE_CLICK:
            return "BUTTON_EVENT_DOUBLE_CLICK";
        case BUTTON_EVENT_TRIPLE_CLICK:
            return "BUTTON_EVENT_TRIPLE_CLICK";
        case BUTTON_EVENT_FIVE_CLICKS:
            return "BUTTON_EVENT_FIVE_CLICKS";
        case BUTTON_EVENT_LONG_PRESS_HOLD:
            return "BUTTON_EVENT_LONG_PRESS_HOLD";
        case BUTTON_EVENT_FACTORY_RESET_5S:
            return "BUTTON_EVENT_FACTORY_RESET_5S";
        default:
            return "BUTTON_EVENT_UNKNOWN";
    }
}

const char* ButtonDriver::eventToString(ButtonEvent event) {
    return buttonEventToString(event);
}

ButtonDriver::ButtonDriver(uint8_t pin,
                           uint8_t activeLevel,
                           uint32_t debounceMs,
                           uint32_t clickTimeoutMs,
                           uint32_t longPressMs,
                           uint32_t holdIntervalMs)
    : _pin(pin),
      _activeLevel(activeLevel),
      _debounceMs(debounceMs),
      _clickTimeoutMs(clickTimeoutMs),
      _longPressMs(longPressMs),
      _holdIntervalMs(holdIntervalMs),
      _rawReading(false),
      _lastRawReading(false),
      _debouncedState(false),
      _lastDebounceTime(0),
      _pressStartTime(0),
      _lastReleaseTime(0),
      _clickCount(0),
      _longPressFired(false),
      _isFirstHoldTick(false),
      _lastHoldTickTime(0),
      _callback(nullptr) {}

void ButtonDriver::begin() {
    pinMode(_pin, _activeLevel == HIGH ? INPUT_PULLDOWN : INPUT_PULLUP);
    _rawReading = (digitalRead(_pin) == _activeLevel);
    _lastRawReading = _rawReading;
    _debouncedState = _rawReading;
    _lastDebounceTime = millis();
    _pressStartTime = _debouncedState ? millis() : 0;
    _lastReleaseTime = 0;
    _clickCount = 0;
    _longPressFired = false;
    _isFirstHoldTick = false;
    _lastHoldTickTime = 0;
}

bool ButtonDriver::isPressed() const {
    return _debouncedState;
}

bool ButtonDriver::isHeld() const {
    if (!_debouncedState) {
        return false;
    }
    return (millis() - _pressStartTime) >= _longPressMs;
}

uint32_t ButtonDriver::getHoldDurationMs() const {
    if (!_debouncedState) {
        return 0;
    }
    return millis() - _pressStartTime;
}

void ButtonDriver::setCallback(ButtonCallback callback) {
    _callback = callback;
}

ButtonEvent ButtonDriver::update() {
    uint32_t now = millis();
    ButtonEvent event = BUTTON_EVENT_NONE;

    // 1. Read raw pin state and debounce
    bool reading = (digitalRead(_pin) == _activeLevel);

    if (reading != _lastRawReading) {
        _lastDebounceTime = now;
        _lastRawReading = reading;
    }

    if ((now - _lastDebounceTime) >= _debounceMs) {
        if (reading != _debouncedState) {
            _debouncedState = reading;
            if (_debouncedState) {
                // Button transition: PRESSED
                _pressStartTime = now;
                _longPressFired = false;
                _isFirstHoldTick = false;
                _lastHoldTickTime = 0;
            } else {
                // Button transition: RELEASED
                _isFirstHoldTick = false;
                if (_longPressFired) {
                    // Holding state already triggered; do not register clicks on release
                    _clickCount = 0;
                    _longPressFired = false;
                } else {
                    // Valid short click release
                    _clickCount++;
                    _lastReleaseTime = now;
                    if (_clickCount >= 5) {
                        event = BUTTON_EVENT_FIVE_CLICKS;
                        _clickCount = 0;
                    }
                }
            }
        }
    }

    // 2. Continuous hold detection while physically pressed (HIGH)
    if (_debouncedState) {
        uint32_t holdTime = now - _pressStartTime;
        if (holdTime >= _longPressMs) {
            if (!_longPressFired) {
                // First hold tick at 700 ms
                _longPressFired = true;
                _isFirstHoldTick = true;
                _clickCount = 0;
                _lastHoldTickTime = now;
                event = BUTTON_EVENT_LONG_PRESS_HOLD;
            } else if ((now - _lastHoldTickTime) >= _holdIntervalMs) {
                // Subsequent hold ticks every 40 ms
                _isFirstHoldTick = false;
                _lastHoldTickTime = now;
                event = BUTTON_EVENT_LONG_PRESS_HOLD;
            }
        }
    }

    // 3. Multi-click timeout check while released
    if (!_debouncedState && _clickCount > 0 && event == BUTTON_EVENT_NONE) {
        if ((now - _lastReleaseTime) >= _clickTimeoutMs) {
            if (_clickCount == 1) {
                event = BUTTON_EVENT_SINGLE_CLICK;
            } else if (_clickCount == 2) {
                event = BUTTON_EVENT_DOUBLE_CLICK;
            } else if (_clickCount == 3) {
                event = BUTTON_EVENT_TRIPLE_CLICK;
            } else if (_clickCount >= 5) {
                event = BUTTON_EVENT_FIVE_CLICKS;
            }
            _clickCount = 0;
        }
    }

    // 4. Fire callback if registered and an event occurred
    if (event != BUTTON_EVENT_NONE && _callback) {
        _callback(event);
    }

    return event;
}
