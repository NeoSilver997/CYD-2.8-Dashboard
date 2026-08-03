// pins.h  --  DEPRECATED alias. The pin map now lives in board.h.
//
// board.h carries both boards we own (2.4" 2432S024R and 2.8" 2432S028R) in
// one profile selected by CYD_BOARD, plus the panel geometry and the backlight
// helper. This file only survives so older includes keep working; new code
// should include board.h and use the CYD_* names directly.

#pragma once

#include "board.h"

// ---- Legacy names -> board.h ----------------------------------------------
#define TFT_MOSI_PIN   CYD_TFT_MOSI_PIN
#define TFT_MISO_PIN   CYD_TFT_MISO_PIN
#define TFT_SCLK_PIN   CYD_TFT_SCLK_PIN
#define TFT_CS_PIN     CYD_TFT_CS_PIN
#define TFT_DC_PIN     CYD_TFT_DC_PIN
#define TFT_RST_PIN    CYD_TFT_RST_PIN
#define TFT_BL_PIN     CYD_TFT_BL_PIN     // 21 on the 2.8", 27 on the 2.4"

#define TOUCH_MOSI_PIN CYD_TOUCH_MOSI_PIN
#define TOUCH_MISO_PIN CYD_TOUCH_MISO_PIN
#define TOUCH_CLK_PIN  CYD_TOUCH_CLK_PIN
#define TOUCH_CS_PIN   CYD_TOUCH_CS_PIN
#define TOUCH_IRQ_PIN  CYD_TOUCH_IRQ_PIN

#define SD_MOSI_PIN    CYD_SD_MOSI_PIN
#define SD_MISO_PIN    CYD_SD_MISO_PIN
#define SD_SCK_PIN     CYD_SD_SCK_PIN
#define SD_CS_PIN      CYD_SD_CS_PIN

#define LDR_PIN        CYD_LDR_PIN
#define RGB_R_PIN      CYD_RGB_R_PIN
#define RGB_G_PIN      CYD_RGB_G_PIN
#define RGB_B_PIN      CYD_RGB_B_PIN
#define SPEAKER_PIN    CYD_SPEAKER_PIN
