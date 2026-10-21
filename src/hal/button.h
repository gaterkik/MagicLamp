#pragma once

#include <Arduino.h>
#include <functional>
#include "config.h"

#ifndef BUTTON_DEBOUNCE_MS
#define BUTTON_DEBOUNCE_MS 30
#endif

#ifndef BUTTON_CLICK_TIMEOUT_MS
#define BUTTON_CLICK_TIMEOUT_MS 420
#endif

#ifndef BUTTON_LONG_PRESS_MS
#define BUTTON_LONG_PRESS_MS 700
#endif

#ifndef BUTTON_HOLD_INTERVAL_MS
#define BUTTON_HOLD_INTERVAL_MS 40
#endif

#ifndef BUTTON_FACTORY_RESET_MS
#define BUTTON_FACTORY_RESET_MS 5000
#endif

/**
 * @brief Events detected by the TTP223 button driver finite state machine (FSM).
 */
enum ButtonEvent : uint8_t {
    BUTTON_EVENT_NONE = 0,
    BUTTON_EVENT_SINGLE_CLICK,
    BUTTON_EVENT_DOUBLE_CLICK,
    BUTTON_EVENT_TRIPLE_CLICK,
    BUTTON_EVENT_FOUR_CLICKS,
    BUTTON_EVENT_SEVEN_CLICKS,
    BUTTON_EVENT_LONG_PRESS_HOLD,
    BUTTON_EVENT_FACTORY_RESET_5S
};

/**
 * @brief Convert ButtonEvent enum to human-readable string.
 * @param event The event to convert.
 * @return C-string representation of the event name.
 */
const char* buttonEventToString(ButtonEvent event);

/**
 * @brief Callback function signature for button events.
 */
using ButtonCallback = std::function<void(ButtonEvent)>;

/**
 * @brief Non-blocking FSM driver for TTP223 capacitive touch button.
 *
 * Implements software debouncing and event recognition:
 * - Single click: toggle power
 * - Double click: next effect
 * - Triple click: reserved for GitHub Silent OTA / WebUI
 * - Four clicks: force Wi-Fi session
 * - Seven clicks: NVS reset & Wi-Fi reset
 * - Long press hold: triggers at 700 ms, then every 40 ms while held for smooth dimming.
 *
 * Strictly non-blocking: uses millis(), contains zero delay() calls.
 */
class ButtonDriver {
public:
    /**
     * @brief Construct a new ButtonDriver object.
     * @param pin GPIO pin connected to button (default PIN_BUTTON).
     * @param activeLevel Pin state considered active/pressed (default BUTTON_ACTIVE_LEVEL).
     * @param debounceMs Debounce time in ms (default BUTTON_DEBOUNCE_MS).
     * @param clickTimeoutMs Max interval between clicks for multi-click in ms (default BUTTON_CLICK_TIMEOUT_MS).
     * @param longPressMs Duration to trigger LONG_PRESS_HOLD in ms (default BUTTON_LONG_PRESS_MS).
     * @param holdIntervalMs Interval between repeated LONG_PRESS_HOLD events while pressed (default BUTTON_HOLD_INTERVAL_MS).
     */
    ButtonDriver(uint8_t pin = PIN_BUTTON,
                 uint8_t activeLevel = BUTTON_ACTIVE_LEVEL,
                 uint32_t debounceMs = BUTTON_DEBOUNCE_MS,
                 uint32_t clickTimeoutMs = BUTTON_CLICK_TIMEOUT_MS,
                 uint32_t longPressMs = BUTTON_LONG_PRESS_MS,
                 uint32_t holdIntervalMs = BUTTON_HOLD_INTERVAL_MS);

    /**
     * @brief Initialize button GPIO pin.
     */
    void begin();

    /**
     * @brief Poll button state and advance the FSM.
     * Must be called frequently in loop(). Strictly non-blocking.
     * @return ButtonEvent Detected event, or BUTTON_EVENT_NONE.
     */
    ButtonEvent update();

    /**
     * @brief Check if the button is currently physically touched/pressed.
     * @return true if pressed, false otherwise.
     */
    bool isPressed() const;

    /**
     * @brief Check if the button is currently held for longer than longPressMs.
     * @return true if held >= longPressMs, false otherwise.
     */
    bool isHeld() const;

    /**
     * @brief Returns true if the current LONG_PRESS_HOLD event is the very first one for this press session.
     * Used to toggle dimmer direction once per hold session.
     */
    bool isFirstHoldTick() const { return _isFirstHoldTick; }

    /**
     * @brief Get the duration in ms the button has been held.
     * @return Elapsed milliseconds since press, or 0 if not pressed.
     */
    uint32_t getHoldDurationMs() const;

    /**
     * @brief Register an optional callback invoked when any event triggers.
     * @param callback Function or lambda taking ButtonEvent.
     */
    void setCallback(ButtonCallback callback);

    /**
     * @brief Helper to convert event enum to string.
     */
    static const char* eventToString(ButtonEvent event);

private:
    uint8_t _pin;
    uint8_t _activeLevel;
    uint32_t _debounceMs;
    uint32_t _clickTimeoutMs;
    uint32_t _longPressMs;
    uint32_t _holdIntervalMs;

    bool _rawReading;
    bool _lastRawReading;
    bool _debouncedState;
    uint32_t _lastDebounceTime;

    uint32_t _pressStartTime;
    uint32_t _lastReleaseTime;
    uint8_t _clickCount;

    bool _longPressFired;
    bool _isFirstHoldTick;
    uint32_t _lastHoldTickTime;

    ButtonCallback _callback;
};
