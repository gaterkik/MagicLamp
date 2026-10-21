#pragma once

#include <stdint.h>

/**
 * @file config.h
 * @brief Hardware configuration and pinout definition for MagicLamp (ESP32-C6).
 * 
 * Strictly follows ARCHITECTURE.md specifications.
 * DO NOT change pin definitions without consulting ARCHITECTURE.md.
 */

// =============================================================================
// Project & Repository Metadata (ARCHITECTURE.md Section 1)
// =============================================================================

#define PROJECT_REPO_URL        "https://github.com/gaterkik/MagicLamp"
#define PROJECT_REPO_OWNER      "gaterkik"
#define PROJECT_REPO_NAME       "MagicLamp"

// =============================================================================
// Timezone Configuration (ARCHITECTURE.md Section 1)
// =============================================================================

#define TIMEZONE_NAME           "GMT+2"
#define TIMEZONE_OFFSET_SEC     7200

// =============================================================================
// Pin Definitions (Section 1: Hardware & Pinout)
// =============================================================================

/// WS2812B LED Matrix Data In (DIN) - Driven via hardware RMT block
#define PIN_LED_DIN             2

/// TTP223 Capacitive Touch Button (Active HIGH)
#define PIN_BUTTON              7
#define BUTTON_ACTIVE_LEVEL     1

/// Button Timing Parameters (ms)
#define BUTTON_DEBOUNCE_MS      30
#define BUTTON_CLICK_TIMEOUT_MS 420
#define BUTTON_LONG_PRESS_MS    700
#define BUTTON_HOLD_INTERVAL_MS 40
#define BUTTON_FACTORY_RESET_MS 5000

/// INMP441 I2S Digital MEMS Microphone (Standard Mode RX)
#define PIN_MIC_SCK             18  // BCLK (Serial Clock)
#define PIN_MIC_WS              19  // LRCLK (Word Select)
#define PIN_MIC_SD              20  // DIN / DOUT from mic (Serial Data)
// Note: INMP441 L/R pin must be wired to GND (Left channel).
#define MIC_SAMPLE_RATE_HZ      16000
#define MIC_DMA_FRAME_NUM       128
#define MIC_DMA_DESC_NUM        4

// =============================================================================
// LED Matrix Geometry & Display Parameters (Section 3)
// =============================================================================

#define MATRIX_WIDTH            16
#define MATRIX_HEIGHT           16
#define NUM_LEDS                (MATRIX_WIDTH * MATRIX_HEIGHT)  // 256 pixels

/// Serpentine (snake) layout flag: alternate rows run in reverse order
#define MATRIX_SERPENTINE       1

// =============================================================================
// Effects & Storage Parameters
// =============================================================================

#define TOTAL_EFFECTS           15
#define NUM_CORE_EFFECTS        15
#define DEFAULT_CYCLE_MASK      0x7FFF
#define NVS_SAVE_DELAY_MS       3000
#define FADE_DURATION_MS        350
#define DEFAULT_BRIGHTNESS      128

// =============================================================================
// Power Limits (Section 1: Hardware & Pinout)
// =============================================================================

/// External Power Supply: 5.0 V
#define POWER_SUPPLY_VOLTS      5

/// Strict software current limit: 2500 mA (2.5 A). DO NOT EXCEED.
#define CURRENT_LIMIT_MA        2500

// =============================================================================
// System & Telemetry Configuration
// =============================================================================

/// Serial monitor baud rate
#define SERIAL_BAUD_RATE        115200

/// Interval for printing CPU temperature in base firmware (ms)
#define TEMP_PRINT_INTERVAL_MS  5000
