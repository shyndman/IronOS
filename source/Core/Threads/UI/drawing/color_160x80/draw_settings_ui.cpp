/*
 * draw_settings_ui.cpp — Interface category setting screens for HS-02.
 *
 * Purpose:
 *   Renders the eight Interface sub-menu items: Temperature unit (2-detent Celsius /
 *   Fahrenheit selector + live Sleep/Solder/Boost preset impact chips), Display
 *   orientation, Cooldown flashing, Swap +/- keys, Swap < > keys, Screen brightness,
 *   Screen timeout (truthful 5..180s gate-clipped decay bar + "LCD off" badge), and
 *   Boot logo duration.
 *
 * Why it exists:
 *   Isolates display, unit, and button-mapping preferences into the Interface module.
 */

#include "HudTelemetry.hpp"

#ifdef LCD_160x80

#include "configuration.h"

namespace ChaosHud {
namespace {

void drawSettingTempUnit() {
  const bool inF = isFahrenheit();

  drawBox(10, 27, 56, 21, inF ? Color::kCell : Color::kVolts, inF ? Color::kBorder : Color::kNone,
          3);
  drawTextCentered("Celsius", 10, 56, centeredTextY(27, 21, 15), 15,
                   inF ? Color::kMuted : Color::kDarkInk);

  drawBox(69, 27, 83, 21, inF ? Color::kVolts : Color::kCell, inF ? Color::kNone : Color::kBorder,
          3);
  drawTextCentered("Fahrenheit", 69, 83, centeredTextY(27, 21, 15), 15,
                   inF ? Color::kDarkInk : Color::kMuted);

  drawChevrons(Color::kVolts);
  const uint16_t sleepT  = getSettingValue(SettingsOptions::SleepTemp);
  const uint16_t solderT = getSettingValue(SettingsOptions::SolderingTemp);
  const uint16_t boostT  = getSettingValue(SettingsOptions::BoostTemp);

  char sleepBuf[8];
  char solderBuf[8];
  char boostBuf[8];
  appendUnsigned(sleepBuf, sleepT);
  appendUnsigned(solderBuf, solderT);
  if (boostT == 0u) {
    appendStr(boostBuf, "Off");
  } else {
    appendUnsigned(boostBuf, boostT);
  }

  const int16_t chipTextY = centeredTextY(51, 19, 15);
  drawBox(24, 51, 29, 19, Color::kSlpDim, Color::kNone, 3);
  drawTextCentered(sleepBuf, 24, 29, chipTextY, 15, Color::kSlp);

  drawBox(55, 51, 29, 19, Color::kHeatDim, Color::kNone, 3);
  drawTextCentered(solderBuf, 55, 29, chipTextY, 15, Color::kHeat);

  drawBox(86, 51, 30, 19, Color::kRedlineDim, Color::kNone, 3);
  drawTextCentered(boostBuf, 86, 30, chipTextY, 15, Color::kRedline);

  Display::drawSpaceGroteskRgb(tempUnitSuffix(), 118, 49, 19, Color::kInk);
}

void drawSettingDisplayRotation() {
  const uint16_t mode = getSettingValue(SettingsOptions::OrientationMode);

  const bool rightSelected = (mode == 0u);
  drawBox(10, 27, 40, 21, rightSelected ? Color::kVolts : Color::kCell,
          rightSelected ? Color::kNone : Color::kBorder, 3);
  Display::drawSpaceGroteskRgb("R", 19, 28, 15, rightSelected ? Color::kDarkInk : Color::kMuted);
  drawBox(35, 32, 4, 11, Color::kLock, Color::kNone, 1);

  const bool leftSelected = (mode == 1u);
  drawBox(54, 27, 40, 21, leftSelected ? Color::kVolts : Color::kCell,
          leftSelected ? Color::kNone : Color::kBorder, 3);
  drawBox(65, 32, 4, 11, Color::kHeat, Color::kNone, 1);
  Display::drawSpaceGroteskRgb("L", 74, 28, 15, leftSelected ? Color::kDarkInk : Color::kMuted);

  const bool autoSelected = (mode >= 2u);
  drawPill(98, 27, 54, 21, autoSelected ? Color::kVolts : Color::kCell,
           autoSelected ? Color::kNone : Color::kBorder, "Auto",
           autoSelected ? Color::kDarkInk : Color::kMuted);

  drawChevrons(Color::kVolts);
  const char *modeLabel = (mode == 0u) ? "Right" : ((mode == 1u) ? "Left" : "Auto");
  Display::drawSpaceGroteskRgb(modeLabel, 24, 49, 19, Color::kInk);

  const bool     isLeftHanded = Display::getRotation();
  const uint16_t gripFg       = isLeftHanded ? Color::kHeat : Color::kLock;
  const uint16_t gripDim      = isLeftHanded ? Color::kHeatDim : Color::kLockDim;
  drawBox(75, 51, 61, 19, gripDim, gripFg, 4);
  drawBox(81, 58, 4, 4, gripFg, Color::kNone, 1);
  Display::drawSpaceGroteskRgb(isLeftHanded ? "Left" : "Right", 89, 51, 15, gripFg);
}

void drawSettingCooldownBlink() {
  const bool strobe = getSettingValue(SettingsOptions::CoolingTempBlink) != 0u;
  const bool inF    = isFahrenheit();
  uint8_t    heights[10];
  uint16_t   colors[10];

  if (strobe) {
    for (uint8_t i = 0; i < 8u; ++i) {
      const bool high = ((i & 1u) == 0u);
      heights[i]      = high ? 13u : 4u;
      colors[i]       = high ? Color::kWatts : Color::kRedlineDim;
    }
    heights[8] = 13u;
    heights[9] = 4u;
  } else {
    static constexpr uint8_t kSteadyHeights[8] = {13, 12, 10, 9, 8, 7, 6, 5};
    for (uint8_t i = 0; i < 8u; ++i) {
      heights[i] = kSteadyHeights[i];
      colors[i]  = (i < 4u) ? Color::kWatts : Color::kWattsDim;
    }
    heights[8] = 4u;
    heights[9] = 4u;
  }
  colors[8] = Color::kLock;
  colors[9] = Color::kLock;

  drawSegmentBar(Layout::kContentX, 42, Layout::kBarW, 10, heights, colors, -1, Color::kInk, -1,
                 Color::kInk, 0);
  drawChevrons(Color::kVolts);

  drawPill(25, 50, 56, 21, strobe ? Color::kWatts : Color::kCell,
           strobe ? Color::kNone : Color::kBorder, strobe ? "Strobe" : "Steady",
           strobe ? Color::kDarkInk : Color::kInk);
  drawPill(85, 50, 51, 21, Color::kRedlineDim, Color::kRedline,
           inF ? (">122" HS02_SYM_DEG "F") : (">50" HS02_SYM_DEG "C"), Color::kRedline);
}

void drawSettingReverseTempChange() {
  const bool     swapped = getSettingValue(SettingsOptions::ReverseButtonTempChangeEnabled) != 0u;
  const bool     leftIsPlus = isTempButtonsReversed();
  const uint16_t step       = getSettingValue(SettingsOptions::TempChangeLongStep);

  char  plusBuf[12];
  char *pp = appendStr(plusBuf, "+");
  pp       = appendUnsigned(pp, step);
  appendStr(pp, HS02_SYM_DEG);

  char  minusBuf[12];
  char *mp = appendStr(minusBuf, HS02_SYM_MINUS);
  mp       = appendUnsigned(mp, step);
  appendStr(mp, HS02_SYM_DEG);

  drawKeyPaddle(10, 26, 68, 22, "L", leftIsPlus ? Color::kHeat : Color::kVolts, Color::kDarkInk,
                leftIsPlus ? Color::kHeatDim : Color::kVoltsDim,
                leftIsPlus ? Color::kHeat : Color::kVolts, leftIsPlus ? plusBuf : minusBuf,
                leftIsPlus ? Color::kHeat : Color::kVolts);
  drawKeyPaddle(84, 26, 68, 22, "R", leftIsPlus ? Color::kVolts : Color::kHeat, Color::kDarkInk,
                leftIsPlus ? Color::kVoltsDim : Color::kHeatDim,
                leftIsPlus ? Color::kVolts : Color::kHeat, leftIsPlus ? minusBuf : plusBuf,
                leftIsPlus ? Color::kVolts : Color::kHeat);

  drawChevrons(Color::kVolts);
  Display::drawSpaceGroteskRgb(swapped ? "Swapped" : "Normal", 23, 50, 19, Color::kInk);

  drawBox(115, 52, 21, 20, swapped ? Color::kWattsDim : Color::kCell,
          swapped ? Color::kWatts : Color::kBorder, 4);
  Display::drawAaBitmapRgb(Hs02UI::kIconArrowLeftRight.data, Hs02UI::kIconArrowLeftRight.width,
                           Hs02UI::kIconArrowLeftRight.height, 119, 56,
                           swapped ? Color::kWatts : Color::kMuted);
}

void drawSettingReverseSettings() {
  const bool swapped  = getSettingValue(SettingsOptions::ReverseButtonSettings) != 0u;
  const bool nextLeft = isSettingsButtonsReversed();

  drawKeyPaddle(10, 26, 68, 22, "L", nextLeft ? Color::kLock : Color::kSlp, Color::kDarkInk,
                nextLeft ? Color::kLockDim : Color::kSlpDim, nextLeft ? Color::kLock : Color::kSlp,
                nextLeft ? "Next" : "Prev", nextLeft ? Color::kLock : Color::kSlp);
  drawKeyPaddle(84, 26, 68, 22, "R", nextLeft ? Color::kSlp : Color::kLock, Color::kDarkInk,
                nextLeft ? Color::kSlpDim : Color::kLockDim, nextLeft ? Color::kSlp : Color::kLock,
                nextLeft ? "Prev" : "Next", nextLeft ? Color::kSlp : Color::kLock);

  const char *valLabel = swapped ? "Swapped" : "Normal";
  const char *tagLabel = swapped ? "Rev" : "Std";
  drawSettingBottomRow(Color::kVolts, valLabel, tagLabel,
                       swapped ? Color::kWattsDim : Color::kVoltsDim, Color::kNone,
                       swapped ? Color::kWatts : Color::kVolts);
}

void drawSettingBrightness() {
  const uint16_t raw = getSettingValue(SettingsOptions::DisplayBrightness);
  Display::setBrightness(raw);

  const uint8_t level = static_cast<uint8_t>(raw / BRIGHTNESS_STEP + 1u);

  static constexpr uint8_t  kHeights[9] = {4, 5, 7, 8, 9, 11, 12, 13, 15};
  static constexpr uint16_t kRamp[9]    = {
      Color::kSlp,  Color::kSlp,   Color::kVolts, Color::kVolts, Color::kVolts,
      Color::kLock, Color::kWatts, Color::kWatts, Color::kInk,
  };

  uint16_t colors[9];
  for (uint8_t i = 0; i < 9u; ++i) {
    colors[i] = (i < level) ? kRamp[i] : Color::kTrack;
  }

  drawSegmentBar(Layout::kContentX, 41, Layout::kBarW, 9, kHeights, colors, -1, Color::kInk, -1,
                 Color::kInk, 0);
  drawChevrons(Color::kVolts);

  char  valBuf[12];
  char *vp = appendUnsigned(valBuf, level);
  appendStr(vp, " / 9");
  Display::drawSpaceGroteskRgb(valBuf, 25, 49, 19, Color::kInk);

  char  pctBuf[10];
  char *pp = appendUnsigned(pctBuf, (static_cast<uint32_t>(level) * 100u) / 9u);
  appendStr(pp, "%");
  drawIconPill(77, 51, 59, 20, Color::kWatts, Color::kNone, Hs02UI::kIconSun, Color::kDarkInk,
               pctBuf, Color::kDarkInk);
}

// DisplayIdleTimeoutSeconds spans 5..180s in 5s steps; GUIThread blanks only the LCD
// (status LED stays active), so the badge accurately states "LCD off".
void drawSettingDisplayIdleTimeout() {
  static constexpr uint8_t  kLitHeights[12] = {11, 11, 10, 10, 9, 9, 8, 8, 7, 7, 6, 6};
  static constexpr uint16_t kLitColors[12]  = {
      Color::kInk, Color::kWatts, Color::kWatts, Color::kVolts, Color::kVolts, Color::kVolts,
      Color::kSlp, Color::kSlp,   Color::kSlp,   Color::kAdv,   Color::kAdv,   Color::kAdv,
  };

  const uint16_t sec   = getSettingValue(SettingsOptions::DisplayIdleTimeoutSeconds);
  const int16_t  gateX = mapRangeToBarX(static_cast<int32_t>(sec), 5, 180, Layout::kContentX + 4,
                                        Layout::kGateSpanW - 4);
  const int16_t  segW  = segmentWidth(Layout::kBarW, 12);

  for (uint8_t i = 0; i < 12u; ++i) {
    const int16_t segX     = segmentLeftX(Layout::kContentX, Layout::kBarW, 12, i);
    const int16_t segRight = static_cast<int16_t>(segX + segW);
    const uint8_t litH     = kLitHeights[i];
    const int16_t litY     = static_cast<int16_t>(40 - litH);

    if (gateX >= segRight) {
      Display::drawBoxRgb(segX, litY, segW, litH, kLitColors[i], Color::kNone, 1);
    } else if (gateX <= segX) {
      Display::drawBoxRgb(segX, 36, segW, 4, Color::kTrack, Color::kNone, 1);
    } else {
      const int16_t leftW  = static_cast<int16_t>(gateX - segX);
      const int16_t rightX = static_cast<int16_t>(gateX + 2);
      const int16_t rightW = static_cast<int16_t>(segRight - rightX);

      Display::drawBoxRgb(segX, litY, leftW, litH, kLitColors[i], Color::kNone, 1);
      if (rightW > 0) {
        Display::drawBoxRgb(rightX, 36, rightW, 4, Color::kTrack, Color::kNone, 1);
      }
    }
  }

  Display::drawBoxRgb(gateX, 27, 2, 15, Color::kInk, Color::kNone, 0);

  char  valBuf[12];
  char *p = appendUnsigned(valBuf, sec);
  appendStr(p, " s");
  drawSettingBottomRow(Color::kVolts, valBuf, "LCD off", Color::kWattsDim, Color::kWatts,
                       Color::kWatts);
}

void drawSettingLogoTime() {
  static constexpr uint16_t kBarCols[5] = {
      Color::kVolts, Color::kLock, Color::kWatts, Color::kHeat, Color::kInk,
  };

  const uint16_t mode    = getSettingValue(SettingsOptions::LOGOTime);
  const bool     isOff   = (mode == logoMode_t::SKIP);
  const bool     isOnce  = (mode == logoMode_t::ONETIME);
  const bool     isInf   = (mode >= logoMode_t::INFINITY);
  const bool     isTimed = (mode >= 1u && mode <= 4u);

  drawPill(10, 27, 28, 19, isOff ? Color::kRedline : Color::kRedlineDim, Color::kNone, "Off",
           isOff ? Color::kDarkInk : Color::kRedline);

  uint16_t colors[5];
  uint8_t  heights[5];
  for (uint8_t i = 0; i < 5u; ++i) {
    const bool lit = isTimed && (i < mode);
    heights[i]     = lit ? static_cast<uint8_t>(7u + i * 2u) : 6u;
    colors[i]      = lit ? kBarCols[i] : Color::kTrack;
  }
  drawSegmentBar(41, 42, 52, 5, heights, colors, -1, Color::kInk, -1, Color::kInk, 0);

  drawPill(96, 27, 24, 19, isOnce ? Color::kSlp : Color::kSlpDim, Color::kNone, "1x",
           isOnce ? Color::kDarkInk : Color::kSlp);
  drawPill(123, 27, 29, 19, isInf ? Color::kPwr : Color::kPwrDim, Color::kNone, "Inf",
           isInf ? Color::kDarkInk : Color::kPwr);

  if (isOff) {
    drawSettingBottomRow(Color::kVolts, "Off", "Skip", Color::kRedlineDim, Color::kNone,
                         Color::kRedline);
  } else if (isOnce) {
    drawSettingBottomRow(Color::kVolts, "Once", "Once", Color::kSlpDim, Color::kNone, Color::kSlp);
  } else if (isInf) {
    drawSettingBottomRow(Color::kVolts, "Inf", "Loop", Color::kPwrDim, Color::kNone, Color::kPwr);
  } else {
    char  valBuf[12];
    char *p = appendUnsigned(valBuf, mode);
    appendStr(p, " s");
    drawSettingBottomRow(Color::kVolts, valBuf, "Splash", Color::kLock, Color::kNone,
                         Color::kDarkInk);
  }
}

} // namespace

bool drawUiSetting(SettingsItemIndex idx) {
  switch (idx) {
  case SettingsItemIndex::TemperatureUnit:
    drawSettingTempUnit();
    return true;
  case SettingsItemIndex::DisplayRotation:
    drawSettingDisplayRotation();
    return true;
  case SettingsItemIndex::CooldownBlink:
    drawSettingCooldownBlink();
    return true;
  case SettingsItemIndex::ReverseButtonTempChange:
    drawSettingReverseTempChange();
    return true;
  case SettingsItemIndex::ReverseButtonSettings:
    drawSettingReverseSettings();
    return true;
  case SettingsItemIndex::Brightness:
    drawSettingBrightness();
    return true;
  case SettingsItemIndex::DisplayIdleTimeout:
    drawSettingDisplayIdleTimeout();
    return true;
  case SettingsItemIndex::LOGOTime:
    drawSettingLogoTime();
    return true;
  default:
    return false;
  }
}

} // namespace ChaosHud

#endif // LCD_160x80
