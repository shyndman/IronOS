/*
 * draw_confirmation_modal.cpp — Full-screen confirmation modals for HS-02.
 *
 * Purpose:
 *   Renders the two destructive/hardware-state confirmation modals invoked by
 *   userConfirmation() in settingsGUI.cpp: factory reset ("Reset settings?") and
 *   cold-junction calibration pre-check ("Tip & handle cold?").
 *
 * Why it exists:
 *   Separates blocking modal dialogs from normal non-blocking setting cards.
 */

#include "HudTelemetry.hpp"

#ifdef LCD_160x80

#include "BSP.h"
#include "TipThermoModel.h"

namespace ChaosHud {
namespace {

constexpr int16_t  kModalButtonY    = 46;
constexpr int16_t  kModalButtonW    = 68;
constexpr int16_t  kModalButtonH    = 24;
constexpr int16_t  kLeftButtonX     = 10;
constexpr int16_t  kRightButtonX    = 84;
constexpr int16_t  kIconTextGap     = 4;
constexpr uint32_t kHotTipThreshold = 50u;

void drawCenteredIconButton(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t fillRgb,
                            uint16_t strokeRgb, const Hs02UI::Icon2bpp &icon, uint16_t iconRgb,
                            const char *label, uint16_t textRgb, bool iconOnRight) {
  drawBox(x, y, w, h, fillRgb, strokeRgb, 4);

  const int16_t labelW = static_cast<int16_t>(Display::measureSpaceGrotesk(label, 15));
  const int16_t totalW = static_cast<int16_t>(icon.width + kIconTextGap + labelW);
  const int16_t startX = static_cast<int16_t>(x + (w - totalW) / 2);
  const int16_t iconY  = static_cast<int16_t>(y + (h - icon.height) / 2);
  const int16_t textY  = centeredTextY(y, h, 15);

  if (iconOnRight) {
    Display::drawSpaceGroteskRgb(label, startX, textY, 15, textRgb);
    const int16_t iconX = static_cast<int16_t>(startX + labelW + kIconTextGap);
    Display::drawAaBitmapRgb(icon.data, icon.width, icon.height, iconX, iconY, iconRgb);
  } else {
    Display::drawAaBitmapRgb(icon.data, icon.width, icon.height, startX, iconY, iconRgb);
    const int16_t textX = static_cast<int16_t>(startX + icon.width + kIconTextGap);
    Display::drawSpaceGroteskRgb(label, textX, textY, 15, textRgb);
  }
}

void drawResetDefaultsModal() {
  drawSolidRail(Color::kRedline);

  Display::drawAaBitmapRgb(Hs02UI::kIconTriangleAlert.data, Hs02UI::kIconTriangleAlert.width,
                           Hs02UI::kIconTriangleAlert.height, 10, 6, Color::kRedline);
  Display::drawSpaceGroteskRgb("Reset settings?", 32, 6, 15, Color::kInk);

  for (uint8_t i = 0; i < 10u; ++i) {
    const int16_t  dashX  = static_cast<int16_t>(10 + i * 14);
    const uint16_t dashFg = ((i & 1u) != 0u) ? Color::kRedlineDim : Color::kRedline;
    Display::drawBoxRgb(dashX, 32, 12, 4, dashFg, Color::kNone, 1);
  }

  drawCenteredIconButton(kLeftButtonX, kModalButtonY, kModalButtonW, kModalButtonH,
                         Color::kRedlineDim, Color::kRedline, Hs02UI::kIconRotateCcw,
                         Color::kRedline, "OK", Color::kRedline, false);
  drawCenteredIconButton(kRightButtonX, kModalButtonY, kModalButtonW, kModalButtonH, Color::kCell,
                         Color::kVolts, Hs02UI::kIconXSmall, Color::kVolts, "Hold", Color::kVolts,
                         true);
}

void drawCalibrateCjcModal() {
  drawSolidRail(Color::kAdv);
  Display::drawSpaceGroteskRgb("Tip & handle\ncold?", 10, 4, 15, Color::kInk);

  const uint32_t handleCx10 = getHandleTemperature(0);
  const uint32_t tipC       = TipThermoModel::getTipInC();

  char     tempBuf[16];
  uint16_t pillFill = Color::kLockDim;
  uint16_t pillText = Color::kLock;

  if (tipC >= kHotTipThreshold) {
    char *p = appendUnsigned(tempBuf, tipC);
    appendStr(p, HS02_SYM_DEG);
    pillFill = Color::kRedlineDim;
    pillText = Color::kRedline;
  } else {
    appendTenths(tempBuf, handleCx10, HS02_SYM_DEG);
  }

  drawPill(104, 5, 49, 19, pillFill, pillText, tempBuf, pillText);

  drawCenteredIconButton(kLeftButtonX, kModalButtonY, kModalButtonW, kModalButtonH, Color::kLockDim,
                         Color::kLock, Hs02UI::kIconCheckSmall, Color::kLock, "OK", Color::kLock,
                         false);
  drawCenteredIconButton(kRightButtonX, kModalButtonY, kModalButtonW, kModalButtonH, Color::kCell,
                         Color::kBorder, Hs02UI::kIconXSmall, Color::kMuted, "Hold", Color::kInk,
                         true);
}

} // namespace

void drawConfirmationModal(bool isResetSettings) {
  Display::clearScreenColor();
  if (isResetSettings) {
    drawResetDefaultsModal();
  } else {
    drawCalibrateCjcModal();
  }
}

} // namespace ChaosHud

#endif // LCD_160x80
