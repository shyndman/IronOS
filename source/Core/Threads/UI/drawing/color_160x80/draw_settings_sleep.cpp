/*
 * draw_settings_sleep.cpp — Sleep category setting screens for HS-02.
 *
 * Purpose:
 *   Renders the four Sleep sub-menu items: Motion sensitivity (live KXTJ3 IMU delta
 *   bars + WAKE/IDLE latch), Sleep temperature (gate-clipped bar over real 10..450 C
 *   span so solid blocks never protrude past the indicator line), Sleep timeout
 *   (5 hot 10s blocks + 6 small minute blocks strictly behind the gate), and
 *   Shutdown timeout (diminished post-gate shutdown ticks matching the mockup).
 *
 * Why it exists:
 *   Groups motion/standby/shutdown behavior into one focused Sleep category module.
 */

#include "HudTelemetry.hpp"

#ifdef LCD_160x80

#include "main.hpp"

namespace ChaosHud {
namespace {

constexpr int16_t kBottomY = 51;
constexpr int16_t kBottomH = 19;

uint16_t imuSampleColor(uint16_t sens, uint16_t sample) {
  if (sens == 0u) {
    return Color::kTrack;
  }
  if (sample < 260u) {
    return Color::kLock;
  }
  if (sample < 520u) {
    return Color::kVolts;
  }
  if (sample < 780u) {
    return Color::kWatts;
  }
  if (sample < 980u) {
    return Color::kHeat;
  }
  return Color::kRedline;
}

void drawMotionSensitivitySetting() {
  const uint16_t sens = getSettingValue(SettingsOptions::Sensitivity);
  const uint8_t  head = imuHistoryHead;
  uint8_t        heights[12];
  uint16_t       colors[12];

  for (uint8_t i = 0; i < 12u; ++i) {
    const uint16_t sample  = imuDeltaHistory[(head + i) % 12u];
    const uint16_t clamped = (sample > 1100u) ? 1100u : sample;
    const uint32_t height  = 3u + (static_cast<uint32_t>(clamped) * 10u) / 1100u;
    heights[i]             = static_cast<uint8_t>(height > 13u ? 13u : height);
    colors[i]              = imuSampleColor(sens, sample);
  }

  const int16_t gateX =
      (sens > 0u) ? mapRangeToBarX(static_cast<int32_t>(sens), 1, 9, 24, 108) : -1;
  drawSegmentBar(Layout::kContentX, 41, Layout::kBarW, 12, heights, colors, gateX, Color::kInk, -1,
                 Color::kInk, 15);

  if (sens == 0u) {
    drawSettingBottomRow(Color::kSlp, "Off", "IMU Off", Color::kCell, Color::kNone, Color::kMuted);
    return;
  }

  drawChevrons(Color::kSlp);
  char valBuf[8];
  appendUnsigned(valBuf, sens);
  Display::drawSpaceGroteskRgb(valBuf, 25, 49, 19, Color::kInk);

  int32_t gTenths = ((1120 - static_cast<int32_t>(sens) * 120) * 10 + 2048) / 4096;
  if (gTenths < 1) {
    gTenths = 1;
  }

  char gBuf[12];
  appendTenths(gBuf, static_cast<uint32_t>(gTenths), "g");
  drawPill(44, kBottomY, 40, kBottomH, Color::kVoltsDim, Color::kNone, gBuf, Color::kVolts);

  if (imuLatestDelta > imuWakeThreshold) {
    drawPill(88, kBottomY, 47, kBottomH, Color::kLock, Color::kNone, "WAKE", Color::kDarkInk);
  } else {
    drawPill(88, kBottomY, 47, kBottomH, Color::kCell, Color::kBorder, "IDLE", Color::kMuted);
  }
}

// SleepTemp spans 10..300 C (60..580 F), while SolderingTemp reaches 450 C.
// Solid sleep segments are clipped strictly at gate1X so no lit block ever extends
// to the right of the white sleep indicator line.
void drawSleepTempBlock(int16_t segX, int16_t segW, int16_t gate1X, int16_t gate2X, uint8_t index) {
  const int16_t  segRight  = static_cast<int16_t>(segX + segW);
  const uint16_t sleepCol  = (index == 0u) ? Color::kVolts : Color::kSlp;
  const bool     inWorkGap = (segX < gate2X);
  const uint8_t  postH     = inWorkGap ? 6u : 4u;
  const uint16_t postCol   = inWorkGap ? Color::kHeatDim : Color::kTrack;

  if (gate1X >= segRight) {
    Display::drawBoxRgb(segX, 29, segW, 11, sleepCol, Color::kNone, 1);
    return;
  }

  if (gate1X <= segX) {
    Display::drawBoxRgb(segX, static_cast<int16_t>(40 - postH), segW, postH, postCol, Color::kNone,
                        1);
    return;
  }

  const int16_t leftW  = static_cast<int16_t>(gate1X - segX);
  const int16_t rightX = static_cast<int16_t>(gate1X + 2);
  const int16_t rightW = static_cast<int16_t>(segRight - rightX);

  Display::drawBoxRgb(segX, 29, leftW, 11, sleepCol, Color::kNone, 1);
  if (rightW > 0) {
    Display::drawBoxRgb(rightX, static_cast<int16_t>(40 - postH), rightW, postH, postCol,
                        Color::kNone, 1);
  }
}

void drawSleepTempSetting() {
  const uint16_t sleepVal  = getSettingValue(SettingsOptions::SleepTemp);
  const uint16_t solderVal = getSettingValue(SettingsOptions::SolderingTemp);
  const int32_t  sleepC    = settingTempToC(sleepVal);
  const int32_t  solderC   = settingTempToC(solderVal);

  char  workBuf[12];
  char *wp = appendUnsigned(workBuf, solderVal);
  appendStr(wp, HS02_SYM_DEG);
  drawPillRight(152, 5, 19, Color::kHeatDim, Color::kNone, workBuf, Color::kHeat);

  const int16_t gate1X = mapRangeToBarX(sleepC, 10, 450, Layout::kContentX, Layout::kGateSpanW);
  const int16_t gate2X = mapRangeToBarX(solderC, 10, 450, Layout::kContentX, Layout::kGateSpanW);
  const int16_t segW   = segmentWidth(Layout::kBarW, 12);

  for (uint8_t i = 0; i < 12u; ++i) {
    const int16_t segX = segmentLeftX(Layout::kContentX, Layout::kBarW, 12, i);
    drawSleepTempBlock(segX, segW, gate1X, gate2X, i);
  }

  Display::drawBoxRgb(gate1X, 26, 2, 14, Color::kInk, Color::kNone, 0);
  Display::drawBoxRgb(gate2X, 26, 2, 14, Color::kHeat, Color::kNone, 0);

  char  valBuf[16];
  char *vp = appendUnsigned(valBuf, sleepVal);
  vp       = appendStr(vp, " ");
  appendStr(vp, tempUnitSuffix());

  const int32_t delta = static_cast<int32_t>(sleepVal) - static_cast<int32_t>(solderVal);
  char          deltaBuf[14];
  char         *dp = deltaBuf;
  if (delta < 0) {
    dp = appendStr(dp, HS02_SYM_MINUS);
    dp = appendUnsigned(dp, static_cast<uint32_t>(-delta));
  } else {
    *dp++ = '+';
    dp    = appendUnsigned(dp, static_cast<uint32_t>(delta));
  }
  appendStr(dp, HS02_SYM_DEG);

  drawSettingBottomRow(Color::kSlp, valBuf, deltaBuf, Color::kHeatDim, Color::kHeat, Color::kHeat);
}

// SleepTime steps: 0=Off, 1..5 = 10s..50s (5 large hot blocks), 6..15 = 1m..10m (6 small blocks).
// For val=6..15, gateX advances linearly from x=70 (right edge of small block 0) to x=115
// (right edge of small block 5), and each small block lights/clips strictly up to gateX so no
// small block ever renders ahead of the indicator line.
void drawSleepTimeoutBlocks(uint16_t val, int16_t gateX) {
  static constexpr uint16_t kHotColors[5] = {
      Color::kHeat, Color::kHeat, Color::kWatts, Color::kWatts, Color::kVolts,
  };

  for (uint8_t i = 0; i < 5u; ++i) {
    const bool lit = (val >= 6u) || (val > 0u && i < val);
    Display::drawBoxRgb(static_cast<int16_t>(10 + i * 10), 30, 8, 10,
                        lit ? kHotColors[i] : Color::kTrack, Color::kNone, 1);
  }

  for (uint8_t j = 0; j < 6u; ++j) {
    const int16_t bx     = static_cast<int16_t>(63 + j * 9);
    const int16_t bRight = static_cast<int16_t>(bx + 7);
    if (val < 6u || gateX <= bx) {
      Display::drawBoxRgb(bx, 32, 7, 6, Color::kSlpDim, Color::kNone, 1);
    } else if (gateX >= bRight) {
      Display::drawBoxRgb(bx, 32, 7, 6, Color::kSlp, Color::kNone, 1);
    } else {
      Display::drawBoxRgb(bx, 32, 7, 6, Color::kSlpDim, Color::kNone, 1);
      Display::drawBoxRgb(bx, 32, static_cast<int16_t>(gateX - bx), 6, Color::kSlp, Color::kNone,
                          1);
    }
  }
}

void drawSleepTimeoutSetting() {
  const uint16_t val   = getSettingValue(SettingsOptions::SleepTime);
  int16_t        gateX = -1;
  if (val >= 1u && val <= 5u) {
    gateX = static_cast<int16_t>(10 + (val - 1u) * 10 + 8);
  } else if (val >= 6u) {
    gateX = mapRangeToBarX(static_cast<int32_t>(val), 6, 15, 70, 45);
  }

  drawSleepTimeoutBlocks(val, gateX);
  drawPill(121, 26, 31, 17, Color::kRedlineDim, Color::kNone, "0W", Color::kRedline);
  if (gateX >= 0) {
    Display::drawBoxRgb(gateX, 27, 2, 15, Color::kInk, Color::kNone, 0);
  }

  if (val == 0u) {
    drawSettingBottomRow(Color::kSlp, "Off", "Stay hot", Color::kCell, Color::kNone, Color::kMuted);
    return;
  }

  char valBuf[14];
  if (val < 6u) {
    char *p = appendUnsigned(valBuf, val * 10u);
    appendStr(p, " s");
  } else {
    char *p = appendUnsigned(valBuf, val - 5u);
    appendStr(p, " min");
  }

  char  targetBuf[16];
  char *tp = appendStr(targetBuf, HS02_SYM_ARROW_R " ");
  tp       = appendUnsigned(tp, getSettingValue(SettingsOptions::SleepTemp));
  appendStr(tp, HS02_SYM_DEG);

  drawSettingBottomRow(Color::kSlp, valBuf, targetBuf, Color::kSlp, Color::kNone, Color::kDarkInk);
}

// ShutdownTime spans 1..60 minutes (0 = Off). Pre-gate blocks sit at 9px height (with an amber
// pre-cutoff warning block right at the gate); post-gate blocks step down to diminished 5px ticks.
void drawShutdownTimeoutSetting() {
  const uint16_t val  = getSettingValue(SettingsOptions::ShutdownTime);
  const int16_t  segW = segmentWidth(Layout::kBarW, 12);

  if (val == 0u) {
    uint8_t  heights[12];
    uint16_t colors[12];
    for (uint8_t i = 0; i < 12u; ++i) {
      heights[i] = 5u;
      colors[i]  = Color::kTrack;
    }
    drawSegmentBar(Layout::kContentX, 39, Layout::kBarW, 12, heights, colors, -1, Color::kInk, -1,
                   Color::kInk, 0);
    drawSettingBottomRow(Color::kSlp, "Off", "Stay on", Color::kCell, Color::kNone, Color::kMuted);
    return;
  }

  const int16_t gateX = mapRangeToBarX(static_cast<int32_t>(val), 1, 60, Layout::kContentX + 6,
                                       Layout::kGateSpanW - 6);

  uint8_t postIndex = 0u;
  for (uint8_t i = 0; i < 12u; ++i) {
    const int16_t segX     = segmentLeftX(Layout::kContentX, Layout::kBarW, 12, i);
    const int16_t segRight = static_cast<int16_t>(segX + segW);

    if (gateX >= segRight) {
      const bool     justBeforeGate = (gateX - segRight) < (segW + 2);
      const uint16_t preCol         = justBeforeGate ? Color::kWatts : Color::kSlp;
      Display::drawBoxRgb(segX, 30, segW, 9, preCol, Color::kNone, 1);
    } else if (gateX <= segX) {
      const uint16_t postCol = ((postIndex & 1u) == 0u) ? Color::kRedline : Color::kRedlineDim;
      ++postIndex;
      Display::drawBoxRgb(segX, 32, segW, 5, postCol, Color::kNone, 1);
    } else {
      const int16_t leftW  = static_cast<int16_t>(gateX - segX);
      const int16_t rightX = static_cast<int16_t>(gateX + 2);
      const int16_t rightW = static_cast<int16_t>(segRight - rightX);

      Display::drawBoxRgb(segX, 30, leftW, 9, Color::kWatts, Color::kNone, 1);
      if (rightW > 0) {
        Display::drawBoxRgb(rightX, 32, rightW, 5, Color::kRedline, Color::kNone, 1);
        ++postIndex;
      }
    }
  }

  Display::drawBoxRgb(gateX, 27, 2, 15, Color::kInk, Color::kNone, 0);

  char  valBuf[14];
  char *p = appendUnsigned(valBuf, val);
  appendStr(p, " min");
  drawSettingBottomRow(Color::kSlp, valBuf, "OFF", Color::kRedline, Color::kNone, Color::kDarkInk);
}

} // namespace

bool drawSleepSetting(SettingsItemIndex idx) {
  switch (idx) {
  case SettingsItemIndex::MotionSensitivity:
    drawMotionSensitivitySetting();
    return true;
  case SettingsItemIndex::SleepTemperature:
    drawSleepTempSetting();
    return true;
  case SettingsItemIndex::SleepTimeout:
    drawSleepTimeoutSetting();
    return true;
  case SettingsItemIndex::ShutdownTimeout:
    drawShutdownTimeoutSetting();
    return true;
  default:
    return false;
  }
}

} // namespace ChaosHud

#endif // LCD_160x80
