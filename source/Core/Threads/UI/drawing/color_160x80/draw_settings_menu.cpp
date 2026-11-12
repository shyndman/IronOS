/*
 * draw_settings_menu.cpp — Category menu cards, sub-setting shell, and help marquee.
 *
 * Purpose:
 *   Renders the five top-level Settings category cards (Power, Soldering, Sleep,
 *   Interface, Advanced), wraps individual sub-menu setting screens with their
 *   category rail + title, renders the scrolling help marquee after inactivity, and
 *   computes the exact tick duration required for three complete help scroll passes.
 *
 * Why it exists:
 *   Centralizes Settings chrome and help-marquee timing so SettingsMenu.cpp and
 *   GUIThread.cpp share one authoritative scroll-cycle definition.
 */

#include "HudTelemetry.hpp"

#ifdef LCD_160x80

#include "Setup.h"
#include "main.hpp"

namespace ChaosHud {
namespace {

constexpr int16_t kTitleX         = 10;
constexpr int16_t kCategoryTitleY = 6;
constexpr int16_t kBadgeX         = 112;
constexpr int16_t kBadgeY         = 10;
constexpr int16_t kBadgeW         = 40;
constexpr int16_t kBadgeH         = 42;
constexpr uint8_t kBadgeRadius    = 8;
constexpr int16_t kBadgeIconX     = 118;
constexpr int16_t kBadgeIconY     = 15;

constexpr int16_t kSleepBadgeX  = 117;
constexpr int16_t kSleepBadgeW  = 35;
constexpr int16_t kSleepIconX   = 121;
constexpr int16_t kSummaryPillY = 34;
constexpr int16_t kSummaryPillH = 19;
constexpr int16_t kPillGap      = 3;

constexpr int16_t kPageBarX     = 10;
constexpr int16_t kPageBarStepX = 29;
constexpr int16_t kPageBarSlotW = 25;
constexpr int16_t kPageActiveY  = 64;
constexpr int16_t kPageActiveH  = 8;
constexpr int16_t kPageIdleY    = 68;
constexpr int16_t kPageIdleH    = 4;

constexpr TickType_t kHelpLeadInTicks    = TICKS_SECOND * 4;
constexpr TickType_t kHelpStepTicks      = TICKS_100MS / 3u;
constexpr uint16_t   kHelpViewWidth      = 136;
constexpr uint16_t   kHelpThumbWidth     = 38;
constexpr uint16_t   kHelpThumbTravel    = 142u - kHelpThumbWidth;
constexpr uint32_t   kHelpScrollTailPx   = 12u;
constexpr uint32_t   kHelpPauseSteps     = 25u;
constexpr uint32_t   kHelpMinRunThroughs = 3u;
int16_t drawSummaryPill(int16_t x, uint16_t fillRgb, uint16_t strokeRgb, const char *label,
                        uint16_t textRgb, int16_t padX = 5, int16_t maxRightX = 108,
                        int16_t gap = kPillGap) {
  int16_t w = measurePillWidth(label, 15, padX);
  if (x + w > maxRightX) {
    w = static_cast<int16_t>(maxRightX - x);
  }

  if (w < 16) {
    return x;
  }

  drawPill(x, kSummaryPillY, w, kSummaryPillH, fillRgb, strokeRgb, label, textRgb);
  return static_cast<int16_t>(x + w + gap);
}

void drawCategoryBadge(size_t categoryIndex, const CategoryTheme &theme) {
  static const Hs02UI::Icon2bpp *const kIcons[5] = {
      &Hs02UI::kIconZap,           &Hs02UI::kIconFlame,        &Hs02UI::kIconMoonStar,
      &Hs02UI::kIconPanelsTopLeft, &Hs02UI::kIconCircuitBoard,
  };

  const size_t            safeIdx = (categoryIndex < 5u) ? categoryIndex : 0u;
  const Hs02UI::Icon2bpp &icon    = *kIcons[safeIdx];
  const bool              isSleep = (safeIdx == 2u);
  const int16_t           badgeX  = isSleep ? kSleepBadgeX : kBadgeX;
  const int16_t           badgeW  = isSleep ? kSleepBadgeW : kBadgeW;
  const int16_t           iconX   = isSleep ? kSleepIconX : kBadgeIconX;

  drawBox(badgeX, kBadgeY, badgeW, kBadgeH, theme.dim, theme.fg, kBadgeRadius);
  Display::drawAaBitmapRgb(icon.data, icon.width, icon.height, iconX, kBadgeIconY, theme.fg);

  if (isSleep && DetectedAccelerometerVersion != AccelType::None) {
    Display::drawBoxRgb(133, 44, 4, 4, Color::kLock, Color::kNone, 2);
  } else if (safeIdx == 3u) {
    Display::drawBoxRgb(125, 45, 14, 3, Color::kWatts, Color::kNone, 1);
  }
}

void drawPowerCategoryPills() {
  char vinBuf[12];
  formatLiveVin(vinBuf);
  const int16_t nextX =
      drawSummaryPill(kTitleX, Color::kVoltsDim, Color::kNone, vinBuf, Color::kVolts);

  char srcBuf[10];
  formatPowerSourceTag(srcBuf);
  drawSummaryPill(nextX, Color::kPwrDim, Color::kNone, srcBuf, Color::kPwr);
}

void drawSolderingCategoryPills() {
  char  solderBuf[12];
  char *p = appendUnsigned(solderBuf, getSettingValue(SettingsOptions::SolderingTemp));
  appendStr(p, HS02_SYM_DEG);
  const int16_t nextX =
      drawSummaryPill(kTitleX, Color::kHeatDim, Color::kNone, solderBuf, Color::kHeat);

  const uint16_t boost = getSettingValue(SettingsOptions::BoostTemp);
  if (boost > 0u) {
    char boostBuf[14];
    boostBuf[0] = '+';
    char *bp    = appendUnsigned(boostBuf + 1, boost);
    appendStr(bp, HS02_SYM_DEG);
    drawSummaryPill(nextX, Color::kRedlineDim, Color::kNone, boostBuf, Color::kRedline);
  } else {
    drawSummaryPill(nextX, Color::kCell, Color::kNone, "Off", Color::kMuted);
  }
}

void drawSleepCategoryPills() {
  char  tempBuf[12];
  char *tp = appendUnsigned(tempBuf, getSettingValue(SettingsOptions::SleepTemp));
  appendStr(tp, HS02_SYM_DEG);

  char           timeBuf[10];
  const uint16_t sleepTime = getSettingValue(SettingsOptions::SleepTime);
  if (sleepTime == 0u) {
    appendStr(timeBuf, "Off");
  } else if (sleepTime < 6u) {
    char *sp = appendUnsigned(timeBuf, sleepTime * 10u);
    appendStr(sp, "s");
  } else {
    char *sp = appendUnsigned(timeBuf, sleepTime - 5u);
    appendStr(sp, "m");
  }

  char           shutBuf[10];
  const uint16_t shutTime = getSettingValue(SettingsOptions::ShutdownTime);
  if (shutTime == 0u) {
    appendStr(shutBuf, "X");
  } else {
    char *kp = appendUnsigned(shutBuf, shutTime);
    appendStr(kp, "m");
  }

  // Adapt horizontal padding from 3px down to 2px when 2-digit shutdown minutes are active
  // so all three pills fit cleanly left of the Sleep moon badge (x <= 114).
  int16_t       padX = 3;
  const int16_t totalAt3 =
      static_cast<int16_t>(measurePillWidth(tempBuf, 15, 3) + measurePillWidth(timeBuf, 15, 3) +
                           measurePillWidth(shutBuf, 15, 3) + 4);
  if (kTitleX + totalAt3 > 114) {
    padX = 2;
  }

  int16_t nextX =
      drawSummaryPill(kTitleX, Color::kSlp, Color::kNone, tempBuf, Color::kDarkInk, padX, 114, 2);
  nextX =
      drawSummaryPill(nextX, Color::kVoltsDim, Color::kVolts, timeBuf, Color::kVolts, padX, 114, 2);
  drawSummaryPill(nextX, Color::kRedlineDim, Color::kNone, shutBuf, Color::kRedline,
                  (shutTime == 0u) ? 5 : padX, 114, 2);
}

void drawInterfaceCategoryPills() {
  const char *unitStr = tempUnitSuffix();
  int16_t     nextX =
      drawSummaryPill(kTitleX, Color::kVolts, Color::kNone, unitStr, Color::kDarkInk, 4);

  const uint16_t orient    = getSettingValue(SettingsOptions::OrientationMode);
  const char    *orientStr = (orient == 2u) ? "IMU" : ((orient == 1u) ? "L" : "R");
  nextX = drawSummaryPill(nextX, Color::kLockDim, Color::kLock, orientStr, Color::kLock, 4);

  uint16_t bStep = getSettingValue(SettingsOptions::DisplayBrightness) / BRIGHTNESS_STEP + 1u;
  if (bStep < 1u) {
    bStep = 1u;
  }
  if (bStep > 9u) {
    bStep = 9u;
  }

  char bBuf[8];
  bBuf[0] = 'B';
  appendUnsigned(bBuf + 1, bStep);
  drawSummaryPill(nextX, Color::kWattsDim, Color::kNone, bBuf, Color::kWatts, 4);
}

void drawAdvancedCategoryPills() {
  char           pwrBuf[12];
  const uint16_t pwrLimit = getSettingValue(SettingsOptions::PowerLimit);
  if (pwrLimit == 0u) {
    appendStr(pwrBuf, "MAX");
  } else {
    char *p = appendUnsigned(pwrBuf, pwrLimit);
    appendStr(p, "W");
  }

  const int16_t nextX = drawSummaryPill(kTitleX, Color::kAdvDim, Color::kNone, pwrBuf, Color::kAdv);

  char divBuf[10];
  appendUnsigned(divBuf, getSettingValue(SettingsOptions::VoltageDiv));
  drawSummaryPill(nextX, Color::kCell, Color::kNone, divBuf, Color::kMuted);
}

void drawBottomPageBar(size_t activeIdx) {
  for (uint8_t i = 0; i < 5u; ++i) {
    const CategoryTheme &tabTheme = categoryTheme(i);
    const int16_t        slotX    = static_cast<int16_t>(kPageBarX + i * kPageBarStepX);
    const bool           active   = (i == activeIdx);

    Display::drawBoxRgb(slotX, active ? kPageActiveY : kPageIdleY, kPageBarSlotW,
                        active ? kPageActiveH : kPageIdleH, active ? tabTheme.fg : tabTheme.dim,
                        Color::kNone, 1);
  }
}

uint32_t helpMaxScrollPx(uint16_t msgWidth) {
  return (msgWidth > kHelpViewWidth)
             ? static_cast<uint32_t>(msgWidth - kHelpViewWidth + kHelpScrollTailPx)
             : 0u;
}

uint32_t helpCycleSteps(uint16_t msgWidth) {
  return kHelpPauseSteps + helpMaxScrollPx(msgWidth) + kHelpPauseSteps;
}

void makeOneLineSettingTitle(SettingsItemIndex idx, char *buf, size_t cap) {
  const char *label =
      (idx == SettingsItemIndex::LockingMode) ? "Button lock" : hs02SettingLabel(idx);

  size_t n = 0;
  while (*label && n + 1u < cap) {
    buf[n++] = (*label == '\n') ? ' ' : *label;
    ++label;
  }
  buf[n] = '\0';
}

void drawSettingHelpMarquee(SettingsItemIndex idx, const CategoryTheme &theme,
                            TickType_t helpElapsedTicks) {
  char title[32];
  makeOneLineSettingTitle(idx, title, sizeof(title));

  char value[32];
  formatSettingSummaryPill(idx, value, sizeof(value));
  const int16_t pillX = drawPillRight(152, 5, 19, theme.dim, Color::kNone, value, theme.fg);
  Display::drawSpaceGroteskRgb(title, kTitleX, 5, 15, Color::kMuted, kTitleX,
                               static_cast<int16_t>(pillX - 3));

  const char    *help      = hs02SettingHelp(idx);
  const uint16_t msgW      = Display::measureSpaceGrotesk(help, 15);
  const uint32_t maxScroll = helpMaxScrollPx(msgW);
  const uint32_t cycle     = helpCycleSteps(msgW);
  const uint32_t step      = (helpElapsedTicks / kHelpStepTicks) % (cycle ? cycle : 1u);
  const uint32_t phase =
      (step < kHelpPauseSteps)
          ? 0u
          : ((step < kHelpPauseSteps + maxScroll) ? (step - kHelpPauseSteps) : maxScroll);

  const int16_t textX = static_cast<int16_t>(16 - static_cast<int32_t>(phase));
  const int16_t thumbX =
      static_cast<int16_t>(10u + ((maxScroll == 0u) ? 0u : (phase * kHelpThumbTravel) / maxScroll));

  Display::drawBoxRgb(10, 30, 142, 2, Color::kTrack);
  Display::drawBoxRgb(thumbX, 30, kHelpThumbWidth, 2, theme.fg);
  drawBox(10, 38, 144, 32, Color::kCell, Color::kBorder, 4);
  Display::drawSpaceGroteskRgb(help, textX, 44, 15, Color::kInk, 15, 149);
}

} // namespace

void drawCategoryMenu(size_t categoryIndex) {
  static const char *const kTitles[5] = {
      "Power", "Soldering", "Sleep", "Interface", "Advanced",
  };

  const size_t         idx   = (categoryIndex < 5u) ? categoryIndex : 0u;
  const CategoryTheme &theme = categoryTheme(static_cast<uint16_t>(idx));

  drawSolidRail(theme.fg);
  Display::drawSpaceGroteskRgb(kTitles[idx], kTitleX, kCategoryTitleY, 19, Color::kInk);
  drawCategoryBadge(idx, theme);

  switch (idx) {
  case 0:
    drawPowerCategoryPills();
    break;
  case 1:
    drawSolderingCategoryPills();
    break;
  case 2:
    drawSleepCategoryPills();
    break;
  case 3:
    drawInterfaceCategoryPills();
    break;
  case 4:
  default:
    drawAdvancedCategoryPills();
    break;
  }

  drawBottomPageBar(idx);
}

void drawSettingSubScreen(SettingsItemIndex idx, uint8_t mainEntry, uint8_t menuLen, uint8_t visIdx,
                          bool showHelp, TickType_t elapsedSinceButton) {
  const CategoryTheme &theme = categoryTheme(mainEntry);
  const uint16_t active = (idx == SettingsItemIndex::SettingsReset) ? Color::kRedline : theme.fg;
  drawSegmentedRail(menuLen, visIdx, active, theme.dim);

  if (!showHelp) {
    const char *label =
        (idx == SettingsItemIndex::LockingMode) ? "Button lock" : hs02SettingLabel(idx);
    bool hasNL = false;
    for (const char *p = label; *p; ++p) {
      if (*p == '\n') {
        hasNL = true;
        break;
      }
    }

    Display::drawSpaceGroteskRgb(label, kTitleX, hasNL ? 4 : 5, 15, Color::kInk, kTitleX, 153);
    drawCustomSettingScreen(idx);
    return;
  }

  const TickType_t helpElapsed =
      (elapsedSinceButton > kHelpLeadInTicks) ? (elapsedSinceButton - kHelpLeadInTicks) : 0u;
  drawSettingHelpMarquee(idx, theme, helpElapsed);
}

TickType_t helpMarqueeMinTimeoutTicks(SettingsItemIndex idx) {
  const char *help = hs02SettingHelp(idx);
  if (help[0] == '\0') {
    return 0;
  }

  const uint16_t msgW       = Display::measureSpaceGrotesk(help, 15);
  const uint32_t totalSteps = kHelpMinRunThroughs * helpCycleSteps(msgW);
  return kHelpLeadInTicks + static_cast<TickType_t>(totalSteps) * kHelpStepTicks;
}

} // namespace ChaosHud

#endif // LCD_160x80
