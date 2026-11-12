/*
 * show_warning.cpp — Fault alerts and transient status latch toasts for HS-02.
 *
 * Purpose:
 *   Implements warnUser() on LCD_160x80, routing hardware faults (Thermal runaway,
 *   Tip shorted, Accelerometer missing) to hazard-striped alert cards and transient
 *   confirmations (Buttons Locked/Unlocked, Calibration Saved, Settings Reset) to
 *   clean status latch toasts.
 *
 * Why it exists:
 *   Provides high-contrast, glanceable fault vs toast differentiation on the 160x80
 *   color panel in place of scrolling monochrome warning strings.
 */

#include "HudWidgets.hpp"
#include "OperatingModeUtilities.h"
#include "OperatingModes.h"
#include "ui_drawing.hpp"

#ifdef LCD_160x80

namespace {

struct FaultLayout {
  uint16_t    accent;
  const char *title;
  const char *detail;
  const char *state;
  uint16_t    stateFill;
  uint16_t    stateText;
  bool        showErr;
};

struct ToastLayout {
  uint16_t                accent;
  uint16_t                dim;
  const char             *category;
  const char             *context;
  const char             *label;
  const Hs02UI::Icon2bpp *icon;
};

void drawFault(const FaultLayout &layout) {
  Display::clearScreenColor();
  ChaosHud::drawSolidRail(layout.accent);
  const uint16_t stripeDim = (layout.accent == ChaosHud::Color::kWatts)
                                 ? ChaosHud::Color::kWattsDim
                                 : ChaosHud::Color::kRedlineDim;
  ChaosHud::drawHazardStripe(layout.accent, stripeDim);

  Display::drawAaBitmapRgb(Hs02UI::kIconTriangleAlert.data, Hs02UI::kIconTriangleAlert.width,
                           Hs02UI::kIconTriangleAlert.height, 9, 8, layout.accent);
  Display::drawSpaceGroteskRgb(layout.title, 29, 7, 15, ChaosHud::Color::kInk);
  Display::drawSpaceGroteskRgb(layout.detail, 10, 29, 19, layout.accent);

  if (layout.state) {
    ChaosHud::drawPill(10, 55, 62, 19, layout.stateFill, ChaosHud::Color::kNone, layout.state,
                       layout.stateText);
  }

  if (layout.showErr) {
    const char *badge =
        layout.state ? (layout.accent == ChaosHud::Color::kWatts ? "FAULT" : "ERR") : "IMU ERR";
    ChaosHud::drawPill(76, 55, 36, 19, stripeDim, ChaosHud::Color::kNone, badge, layout.accent);
  } else if (!layout.state) {
    ChaosHud::drawPill(10, 55, 102, 19, ChaosHud::Color::kWattsDim, ChaosHud::Color::kNone,
                       "IMU ERR", ChaosHud::Color::kWatts);
  }

  ChaosHud::drawPill(116, 55, 36, 19, ChaosHud::Color::kCell, ChaosHud::Color::kBorder, "OK",
                     ChaosHud::Color::kInk);
}

void drawToastLatch(const ToastLayout &toast) {
  constexpr int16_t kCardX   = 10;
  constexpr int16_t kCardY   = 36;
  constexpr int16_t kCardW   = 142;
  constexpr int16_t kCardH   = 34;
  constexpr int16_t kPairGap = 7;
  constexpr int16_t kIconY   = 44;
  constexpr int16_t kLabelY  = 42;

  ChaosHud::drawBox(kCardX, kCardY, kCardW, kCardH, toast.dim, toast.accent, 6);

  const int16_t labelW = static_cast<int16_t>(Display::measureSpaceGrotesk(toast.label, 19));
  const int16_t pairW  = static_cast<int16_t>(toast.icon->width + kPairGap + labelW);
  const int16_t pairX  = static_cast<int16_t>(kCardX + (kCardW - pairW) / 2);

  Display::drawAaBitmapRgb(toast.icon->data, toast.icon->width, toast.icon->height, pairX, kIconY,
                           toast.accent);
  Display::drawSpaceGroteskRgb(toast.label,
                               static_cast<int16_t>(pairX + toast.icon->width + kPairGap), kLabelY,
                               19, toast.accent);
}

void drawToast(const ToastLayout &toast) {
  Display::clearScreenColor();
  ChaosHud::drawSolidRail(toast.accent);
  Display::drawSpaceGroteskRgb(toast.category, 10, 6, 15, ChaosHud::Color::kMuted);
  ChaosHud::drawPill(104, 5, 48, 19, toast.dim, ChaosHud::Color::kNone, toast.context,
                     toast.accent);
  drawToastLatch(toast);
}

bool isLockToast(const char *warning) {
  return warning == translatedString(Tr->LockingKeysString) ||
         warning == translatedString(Tr->WarningKeysLockedString);
}

bool isFault(const char *warning) {
  return warning == translatedString(Tr->WarningThermalRunaway) ||
         warning == translatedString(Tr->WarningTipShorted) ||
         warning == translatedString(Tr->NoAccelerometerMessage);
}

void fillFaultLayout(const char *warning, FaultLayout &out) {
  if (warning == translatedString(Tr->WarningThermalRunaway)) {
    out = {ChaosHud::Color::kRedline, "Thermal runaway",         "Heater cut", "FET OFF",
           ChaosHud::Color::kRedline, ChaosHud::Color::kDarkInk, true};
  } else if (warning == translatedString(Tr->WarningTipShorted)) {
    out = {ChaosHud::Color::kRedline,
           "Tip shorted",
           "Short circuit",
           "FET OFF",
           ChaosHud::Color::kRedline,
           ChaosHud::Color::kDarkInk,
           true};
  } else {
    out = {ChaosHud::Color::kWatts,    "Accelerometer",         "Not detected", nullptr,
           ChaosHud::Color::kWattsDim, ChaosHud::Color::kWatts, false};
  }
}

void fillToastLayout(const char *warning, ToastLayout &out) {
  if (isLockToast(warning)) {
    const char *context = (getSettingValue(SettingsOptions::LockingMode) == 2u) ? "Full" : "Boost";
    out = {ChaosHud::Color::kHeat, ChaosHud::Color::kHeatDim, "Buttons", context, "Locked",
           &Hs02UI::kIconLock};
  } else if (warning == translatedString(Tr->UnlockingKeysString)) {
    out = {ChaosHud::Color::kLock, ChaosHud::Color::kLockDim, "Buttons", "Live", "Unlocked",
           &Hs02UI::kIconLockOpen};
  } else if (warning == translatedString(Tr->CalibrationDone)) {
    out = {ChaosHud::Color::kLock,   ChaosHud::Color::kLockDim, "Calibration", "Flash", "Saved",
           &Hs02UI::kIconCircleCheck};
  } else if (warning == translatedString(Tr->ResetOKMessage)) {
    out = {ChaosHud::Color::kAdv,    ChaosHud::Color::kAdvDim, "Settings", "Default", "Reset OK",
           &Hs02UI::kIconCircleCheck};
  } else {
    out = {ChaosHud::Color::kAdv, ChaosHud::Color::kAdvDim, "Settings", "Default",
           "Defaulted",           &Hs02UI::kIconCircleCheck};
  }
}

} // namespace

bool warnUser(const char *warning, const ButtonState buttons) {
  if (isFault(warning)) {
    FaultLayout fault = {};
    fillFaultLayout(warning, fault);
    drawFault(fault);
  } else if (isLockToast(warning) || warning == translatedString(Tr->UnlockingKeysString) ||
             warning == translatedString(Tr->CalibrationDone) ||
             warning == translatedString(Tr->ResetOKMessage) ||
             warning == translatedString(Tr->SettingsResetMessage)) {
    ToastLayout toast = {};
    fillToastLayout(warning, toast);
    drawToast(toast);
  } else {
    Display::clearScreenColor();
    ChaosHud::drawSolidRail(ChaosHud::Color::kWatts);
    Display::drawAaBitmapRgb(Hs02UI::kIconTriangleAlert.data, Hs02UI::kIconTriangleAlert.width,
                             Hs02UI::kIconTriangleAlert.height, 9, 8, ChaosHud::Color::kWatts);
    Display::drawSpaceGroteskRgb(warning, 29, 7, 15, ChaosHud::Color::kInk);
  }

  if ((xTaskGetTickCount() - lastButtonTime) > TICKS_SECOND * 5) {
    return true;
  }

  return buttons != BUTTON_NONE;
}

#endif // LCD_160x80
