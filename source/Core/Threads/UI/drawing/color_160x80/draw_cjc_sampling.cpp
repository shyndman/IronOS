/*
 * draw_cjc_sampling.cpp — Cold-Junction Compensation sampling progress screen for HS-02.
 *
 * Purpose:
 *   Renders the 4-step CJC calibration progress bar, raw thermocouple microvolts, and
 *   ambient handle temperature while CJC calibration samples at boot.
 *
 * Why it exists:
 *   Implements ui_draw_cjc_sampling() for LCD_160x80.
 */

#include "HudWidgets.hpp"
#include "TipThermoModel.h"
#include "ui_drawing.hpp"

#ifdef LCD_160x80

namespace {

constexpr uint8_t kSamplingSteps    = 4;
constexpr uint8_t kProgressSegments = 12;

void drawSamplingProgress(uint8_t step) {
  const uint8_t filled = static_cast<uint8_t>((step * kProgressSegments) / kSamplingSteps);
  uint16_t      colors[kProgressSegments];

  for (uint8_t i = 0; i < kProgressSegments; ++i) {
    if (i < filled) {
      colors[i] = (i >= 6u) ? ChaosHud::Color::kVolts : ChaosHud::Color::kAdv;
    } else {
      colors[i] = ChaosHud::Color::kTrack;
    }
  }

  ChaosHud::drawUniformBar(10, 32, 142, 7, kProgressSegments, colors);
}

} // namespace

void ui_draw_cjc_sampling(const uint8_t num_dots) {
  Display::clearScreenColor();
  ChaosHud::drawSolidRail(ChaosHud::Color::kAdv);
  Display::drawSpaceGroteskRgb("Sampling CJC", 10, 5, 15, ChaosHud::Color::kInk);

  const uint8_t step = (num_dots > kSamplingSteps) ? kSamplingSteps : num_dots;
  char          stepText[4];
  char         *stepEnd = ChaosHud::appendUnsigned(stepText, step);
  ChaosHud::appendStr(stepEnd, "/4");
  ChaosHud::drawPill(116, 5, 36, 19, ChaosHud::Color::kAdvDim, ChaosHud::Color::kNone, stepText,
                     ChaosHud::Color::kAdv);
  drawSamplingProgress(step);

  char           tipText[20];
  const uint32_t tipMicroVolts = TipThermoModel::convertTipRawADCTouV(getTipRawTemp(0), true);
  char          *tipEnd        = ChaosHud::appendUnsigned(tipText, tipMicroVolts);
  ChaosHud::appendStr(tipEnd, " " HS02_SYM_MICRO "V");
  Display::drawSpaceGroteskRgb(tipText, 10, 54, 15, ChaosHud::Color::kVolts);

  char handleText[20];
  ChaosHud::appendTenths(handleText, getHandleTemperature(0), " " HS02_SYM_DEG "C");
  ChaosHud::drawTextRight(handleText, 152, 54, 15, ChaosHud::Color::kLock);
}

#endif // LCD_160x80
