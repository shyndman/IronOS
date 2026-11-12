/*
 * draw_debug_menu.cpp — Table-driven 17-page hardware & RTOS diagnostic HUD for HS-02.
 *
 * Purpose:
 *   Renders OperatingMode::DebugMenuReadout on LCD_160x80 (firmware build date, 96-bit
 *   MCU ID, accelerometer model, CH224K negotiation status, input voltage, thermocouple
 *   & CJC readings, 20 Hz IMU delta graph, FreeRTOS stack high-water marks, and factory
 *   tip calibration ADC points).
 *
 * Why kDebugPages[] exists:
 *   Binds every page's title and render callback into a single constexpr descriptor
 *   table (checked by static_assert) so reordering or editing pages can never
 *   desynchronize titles from telemetry values.
 */

#include "BSP.h"
#include "HudTelemetry.hpp"
#include "Setup.h"
#include "TipThermoModel.h"
#include "main.hpp"
#include "ui_drawing.hpp"

#ifdef LCD_160x80

extern osThreadId GUITaskHandle;
extern osThreadId MOVTaskHandle;
extern osThreadId PIDTaskHandle;
void              hs02GetFactoryTipCal(uint32_t &adc140, uint32_t &adc240, uint32_t &adc340);

namespace {

constexpr uint8_t  kMeterSegments  = 12;
constexpr uint32_t kTaskStackWords = 512u;

void drawReadout(const char *subtitle, const char *value, uint16_t valueColor) {
  Display::drawSpaceGroteskRgb(subtitle, 10, 28, 15, ChaosHud::Color::kMuted);
  Display::drawSpaceGroteskRgb(value, 10, 51, 19, valueColor);
}

void drawThermalBar(uint32_t celsius) {
  uint16_t      colors[kMeterSegments];
  const uint8_t filled =
      static_cast<uint8_t>((celsius > 450u) ? kMeterSegments : (celsius * kMeterSegments) / 450u);
  for (uint8_t i = 0; i < kMeterSegments; ++i) {
    colors[i] =
        (i < filled) ? ChaosHud::thermalZoneColor(i, kMeterSegments) : ChaosHud::Color::kTrack;
  }

  ChaosHud::drawUniformBar(10, 31, 142, 6, kMeterSegments, colors);
}

void appendHex(char *dst, uint32_t value, uint8_t digits) {
  static constexpr char kHexDigits[] = "0123456789ABCDEF";
  for (uint8_t i = 0; i < digits; ++i) {
    const uint8_t shift = static_cast<uint8_t>((digits - i - 1u) * 4u);
    *dst++              = kHexDigits[(value >> shift) & 0x0Fu];
  }
  *dst = '\0';
}

void renderBuildDatePage() { drawReadout("Compiled", __DATE__, ChaosHud::Color::kInk); }

void renderDeviceIdPage() {
  const uint64_t id = getDeviceID();
  char           value[14];
  appendHex(value, static_cast<uint32_t>((id >> 32) & 0xFFFFu), 4);
  char *p = value + 4;
  *p++    = HS02_SYM_DOT[0];
  appendHex(p, static_cast<uint32_t>(id & 0xFFFFFFFFu), 8);

  Display::drawSpaceGroteskRgb("N32L403 " HS02_SYM_DOT " 64MHz", 10, 28, 15,
                               ChaosHud::Color::kMuted);
  Display::drawSpaceGroteskRgb(value, 10, 52, 17, ChaosHud::Color::kVolts);
}

void renderAccelerometerPage() {
  static const char *const kAccelNames[] = {
      "Scanning", "None",   "MMA8652", "LIS2DH12", "BMA250",
      "MSA301",   "SC7A20", "GPIO",    "LIS2DH12", "KXTJ3",
  };
  const unsigned index = static_cast<unsigned>(DetectedAccelerometerVersion);
  const unsigned count = sizeof(kAccelNames) / sizeof(kAccelNames[0]);

  drawReadout("I2C KXTJ3", (index < count) ? kAccelNames[index] : "None", ChaosHud::Color::kInk);
}

void renderPowerSourcePage() {
  char  request[16];
  char *p =
      ChaosHud::appendUnsigned(request, getSettingValue(SettingsOptions::QCIdealVoltage) / 10u);
  ChaosHud::appendStr(p, "V Req");

  Display::drawSpaceGroteskRgb(CH224KVoltageSettled() ? "Settled" : "Negotiating", 10, 30, 19,
                               ChaosHud::Color::kInk);
  ChaosHud::drawPillRight(152, 52, 19, ChaosHud::Color::kPwrDim, ChaosHud::Color::kNone, request,
                          ChaosHud::Color::kPwr);
}

void renderInputVoltagePage() {
  const uint32_t voltage = ChaosHud::readLiveVinX10();
  const uint32_t request = getSettingValue(SettingsOptions::QCIdealVoltage) / 10u;
  uint16_t       colors[kMeterSegments];
  const uint32_t filled = (voltage > 220u) ? kMeterSegments : (voltage * kMeterSegments) / 220u;

  for (uint8_t i = 0; i < kMeterSegments; ++i) {
    colors[i] = (i < filled) ? ChaosHud::Color::kVolts : ChaosHud::Color::kTrack;
  }
  ChaosHud::drawUniformBar(10, 31, 142, 6, kMeterSegments, colors);

  char value[20];
  ChaosHud::appendTenths(value, voltage, " V");
  Display::drawSpaceGroteskRgb(value, 10, 49, 19, ChaosHud::Color::kVolts);

  char  requestText[18];
  char *p;
  if (CH224KVoltageSettled()) {
    p = ChaosHud::appendUnsigned(requestText, request);
    ChaosHud::appendStr(p, "V OK");
  } else {
    p = ChaosHud::appendStr(requestText, "Req ");
    p = ChaosHud::appendUnsigned(p, request);
    ChaosHud::appendStr(p, "V");
  }

  ChaosHud::drawPillRight(152, 52, 19, ChaosHud::Color::kPwrDim, ChaosHud::Color::kNone,
                          requestText, ChaosHud::Color::kPwr);
}

void renderTipTempPage() {
  const uint32_t tipC = TipThermoModel::getTipInC();
  char           value[20];
  char          *p = ChaosHud::appendUnsigned(value, tipC);
  ChaosHud::appendStr(p, " " HS02_SYM_DEG "C");

  drawThermalBar(tipC);
  Display::drawSpaceGroteskRgb(value, 10, 49, 19, ChaosHud::Color::kInk);
}

void renderHandleTempPage() {
  const uint32_t handleCx10 = getHandleTemperature(0);
  char           value[20];
  ChaosHud::appendTenths(value, handleCx10, " " HS02_SYM_DEG "C");

  drawThermalBar(handleCx10 / 10u);
  Display::drawSpaceGroteskRgb(value, 10, 49, 19, ChaosHud::Color::kInk);
}

void renderMaxTempPage() {
  const uint32_t maxC = TipThermoModel::getTipMaxInC();
  char           value[20];
  char          *p = ChaosHud::appendUnsigned(value, maxC);
  ChaosHud::appendStr(p, " " HS02_SYM_DEG "C");

  drawThermalBar(maxC);
  Display::drawSpaceGroteskRgb(value, 10, 49, 19, ChaosHud::Color::kInk);
}

void renderUptimePage() {
  const uint32_t tenths = static_cast<uint32_t>(xTaskGetTickCount() / TICKS_100MS);
  char           value[20];
  ChaosHud::appendTenths(value, tenths, " s");
  drawReadout("RTOS tick", value, ChaosHud::Color::kInk);
}

void renderMovementPage() {
  uint16_t       colors[kMeterSegments];
  uint8_t        heights[kMeterSegments];
  const uint8_t  head      = imuHistoryHead;
  const uint16_t threshold = imuWakeThreshold ? imuWakeThreshold : 1u;

  for (uint8_t i = 0; i < kMeterSegments; ++i) {
    const uint8_t  historyIndex = static_cast<uint8_t>((head + i) % kMeterSegments);
    const uint16_t delta        = imuDeltaHistory[historyIndex];
    uint16_t       height = static_cast<uint16_t>((static_cast<uint32_t>(delta) * 6u) / threshold);
    if (delta && height == 0u) {
      height = 1u;
    }
    if (height > 6u) {
      height = 6u;
    }

    heights[i] = static_cast<uint8_t>(height);
    colors[i]  = delta ? ChaosHud::Color::kLock : ChaosHud::Color::kTrack;
  }
  ChaosHud::drawSegmentBar(10, 45, 142, kMeterSegments, heights, colors);

  const uint32_t secondsX10 = static_cast<uint32_t>(lastMovementTime / TICKS_100MS);
  char           value[20];
  ChaosHud::appendTenths(value, secondsX10, " s");
  Display::drawSpaceGroteskRgb(value, 10, 49, 19, ChaosHud::Color::kInk);

  char deltaText[16];
  ChaosHud::appendUnsigned(deltaText, imuLatestDelta);
  ChaosHud::drawPillRight(152, 49, 19, ChaosHud::Color::kLockDim, ChaosHud::Color::kNone, deltaText,
                          ChaosHud::Color::kLock);
}

void renderTipResistancePage() {
  char value[20];
  ChaosHud::appendTenths(value, getTipResistanceX10(), " " HS02_SYM_OHM);
  drawReadout("Nominal C245", value, ChaosHud::Color::kInk);
}

void renderRawTipPage() {
  const uint32_t rawUv = TipThermoModel::convertTipRawADCTouV(getTipRawTemp(0), true);
  char           value[20];
  char          *p = ChaosHud::appendUnsigned(value, rawUv);
  ChaosHud::appendStr(p, " " HS02_SYM_MICRO "V");
  drawReadout("Thermocouple", value, ChaosHud::Color::kVolts);
}

void renderColdOffsetPage() {
  char  value[20];
  char *p = ChaosHud::appendUnsigned(value, getSettingValue(SettingsOptions::CalibrationOffset));
  ChaosHud::appendStr(p, " " HS02_SYM_MICRO "V");
  drawReadout("Zero-diff uV", value, ChaosHud::Color::kVolts);
}

void renderTaskStack(TaskHandle_t handle) {
  const uint32_t freeWords = uxTaskGetStackHighWaterMark(handle);
  uint32_t       freePct   = (freeWords * 100u) / kTaskStackWords;
  if (freePct > 100u) {
    freePct = 100u;
  }

  uint16_t       colors[kMeterSegments];
  const uint8_t  filled     = static_cast<uint8_t>((freePct * kMeterSegments) / 100u);
  const uint16_t meterColor = (freePct >= 30u) ? ChaosHud::Color::kLock : ChaosHud::Color::kWatts;

  for (uint8_t i = 0; i < kMeterSegments; ++i) {
    colors[i] = (i < filled) ? meterColor : ChaosHud::Color::kTrack;
  }
  ChaosHud::drawUniformBar(10, 31, 142, 6, kMeterSegments, colors);

  char  value[20];
  char *p = ChaosHud::appendUnsigned(value, freeWords);
  ChaosHud::appendStr(p, " w");
  Display::drawSpaceGroteskRgb(value, 10, 49, 19, ChaosHud::Color::kLock);

  char percent[16];
  p = ChaosHud::appendUnsigned(percent, freePct);
  ChaosHud::appendStr(p, "% ok");
  ChaosHud::drawPillRight(152, 52, 19, ChaosHud::Color::kLockDim, ChaosHud::Color::kNone, percent,
                          ChaosHud::Color::kLock);
}

void renderGuiStackPage() { renderTaskStack(GUITaskHandle); }
void renderMovStackPage() { renderTaskStack(MOVTaskHandle); }
void renderPidStackPage() { renderTaskStack(PIDTaskHandle); }

void renderTipCalPage() {
  uint32_t a140 = 0;
  uint32_t a240 = 0;
  uint32_t a340 = 0;
  hs02GetFactoryTipCal(a140, a240, a340);

  ChaosHud::drawPill(10, 28, 45, 18, ChaosHud::Color::kVoltsDim, ChaosHud::Color::kNone,
                     "140" HS02_SYM_DEG, ChaosHud::Color::kVolts);
  ChaosHud::drawPill(59, 28, 45, 18, ChaosHud::Color::kWattsDim, ChaosHud::Color::kNone,
                     "240" HS02_SYM_DEG, ChaosHud::Color::kWatts);
  ChaosHud::drawPill(107, 28, 45, 18, ChaosHud::Color::kHeatDim, ChaosHud::Color::kNone,
                     "340" HS02_SYM_DEG, ChaosHud::Color::kHeat);

  char  value[32];
  char *p = ChaosHud::appendUnsigned(value, a140);
  p       = ChaosHud::appendStr(p, " " HS02_SYM_DOT " ");
  p       = ChaosHud::appendUnsigned(p, a240);
  p       = ChaosHud::appendStr(p, " " HS02_SYM_DOT " ");
  ChaosHud::appendUnsigned(p, a340);
  Display::drawSpaceGroteskRgb(value, 10, 52, 17, ChaosHud::Color::kInk);
}

struct DebugPageSpec {
  const char *title;
  void (*render)();
};

constexpr DebugPageSpec kDebugPages[] = {
    {    "Build date",     renderBuildDatePage},
    {     "Device ID",      renderDeviceIdPage},
    { "Accelerometer", renderAccelerometerPage},
    {  "Power source",   renderPowerSourcePage},
    { "Input voltage",  renderInputVoltagePage},
    {      "Tip temp",       renderTipTempPage},
    {   "Handle temp",    renderHandleTempPage},
    {      "Max temp",       renderMaxTempPage},
    {        "Uptime",        renderUptimePage},
    {      "Movement",      renderMovementPage},
    {"Tip resistance", renderTipResistancePage},
    {       "Raw tip",        renderRawTipPage},
    {   "Cold offset",    renderColdOffsetPage},
    {     "GUI stack",      renderGuiStackPage},
    {     "MOV stack",      renderMovStackPage},
    {     "PID stack",      renderPidStackPage},
    {       "Tip cal",        renderTipCalPage},
};

constexpr uint8_t kDebugPageCount = sizeof(kDebugPages) / sizeof(kDebugPages[0]);
static_assert(kDebugPageCount == 17u, "DebugMenu.cpp expects 17 HS-02 debug pages");

} // namespace

void ui_draw_debug_menu(const uint8_t item_number) {
  Display::clearScreenColor();
  ChaosHud::drawSolidRail(ChaosHud::Color::kVolts);

  const uint8_t item = static_cast<uint8_t>(item_number % kDebugPageCount);
  Display::drawSpaceGroteskRgb(kDebugPages[item].title, 10, 5, 15, ChaosHud::Color::kInk);

  char  index[8];
  char *p = ChaosHud::appendUnsigned(index, static_cast<uint32_t>(item + 1u));
  ChaosHud::appendStr(p, "/17");
  ChaosHud::drawPillRight(152, 5, 19, ChaosHud::Color::kCell, ChaosHud::Color::kNone, index,
                          ChaosHud::Color::kMuted);

  kDebugPages[item].render();
}

#endif // LCD_160x80
