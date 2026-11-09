#include "Hs02UiAssets.hpp"
#include "OperatingModes.h"
#include "TipThermoModel.h"
#include "main.hpp"
#include "ui_drawing.hpp"
#include <stdio.h>
#ifdef LCD_160x80
extern osThreadId GUITaskHandle;
extern osThreadId MOVTaskHandle;
extern osThreadId PIDTaskHandle;

void ui_draw_debug_menu(const uint8_t item_number) {
  static const char *const titles[17] = {"Build date", "Device ID", "Accelerometer", "Power source", "Input voltage", "Tip temp", "Handle temp", "Max temp", "Uptime", "Movement", "Tip resistance", "Raw tip", "CJC offset", "GUI stack", "MOV stack", "PID stack", "Tip cal"};
  Display::setColorPalette(Hs02UI::kPaletteHomeIdle);
  Display::drawRoundedBarColor(0, 0, 3, 80, 2);
  const uint8_t item = item_number % 17;
  Display::drawSpaceGrotesk(titles[item], 11, 7, 15, 0);
  char index[8];
  snprintf(index, sizeof(index), "%u/17", static_cast<unsigned>(item + 1));
  Display::drawSpaceGrotesk(index, 152 - Display::measureSpaceGrotesk(index, 15), 7, 15, 2);
  char value[40] = {};
  uint32_t tmp;
  switch (item) {
  case 0: snprintf(value, sizeof(value), "%s", __DATE__); break;
  case 1: {
    const uint64_t id = getDeviceID();
    snprintf(value, sizeof(value), "%04X" HS02_SYM_DOT "%08X", static_cast<unsigned>((id >> 32) & 0xFFFF), static_cast<unsigned>(id & 0xFFFFFFFF));
  } break;
  case 2: {
    static const char *const accel[] = {"Scanning", "None", "MMA8652", "LIS2DH12", "BMA250", "MSA301", "SC7A20", "GPIO", "LIS2DH12", "KXTJ3"};
    const unsigned a = static_cast<unsigned>(DetectedAccelerometerVersion);
    snprintf(value, sizeof(value), "%s", a < sizeof(accel) / sizeof(accel[0]) ? accel[a] : "None");
  } break;
  case 3: {
    static const char *const power[] = {"DC", "QC", "PD", "PD"};
    const int8_t p = getPowerSourceNumber();
    snprintf(value, sizeof(value), "%s", (p >= 0 && p < 4) ? power[p] : "DC");
  } break;
  case 4: tmp = getInputVoltageX10(getSettingValue(SettingsOptions::VoltageDiv), 0); snprintf(value, sizeof(value), "%u.%u V", static_cast<unsigned>(tmp / 10), static_cast<unsigned>(tmp % 10)); break;
  case 5: snprintf(value, sizeof(value), "%u " HS02_SYM_DEG "C", static_cast<unsigned>(TipThermoModel::getTipInC())); break;
  case 6: tmp = getHandleTemperature(0); snprintf(value, sizeof(value), "%u.%u " HS02_SYM_DEG "C", static_cast<unsigned>(tmp / 10), static_cast<unsigned>(tmp % 10)); break;
  case 7: snprintf(value, sizeof(value), "%u " HS02_SYM_DEG "C", static_cast<unsigned>(TipThermoModel::getTipMaxInC())); break;
  case 8: snprintf(value, sizeof(value), "%u.%u s", static_cast<unsigned>((xTaskGetTickCount() / TICKS_100MS) / 10), static_cast<unsigned>((xTaskGetTickCount() / TICKS_100MS) % 10)); break;
  case 9: snprintf(value, sizeof(value), "%u.%u s", static_cast<unsigned>((lastMovementTime / TICKS_100MS) / 10), static_cast<unsigned>((lastMovementTime / TICKS_100MS) % 10)); break;
  case 10: tmp = getTipResistanceX10(); snprintf(value, sizeof(value), "%u.%u " HS02_SYM_OHM, static_cast<unsigned>(tmp / 10), static_cast<unsigned>(tmp % 10)); break;
  case 11: snprintf(value, sizeof(value), "%u " HS02_SYM_MICRO "V", static_cast<unsigned>(TipThermoModel::convertTipRawADCTouV(getTipRawTemp(0), true))); break;
  case 12: snprintf(value, sizeof(value), "%u " HS02_SYM_MICRO "V", static_cast<unsigned>(getSettingValue(SettingsOptions::CalibrationOffset))); break;
  case 13: snprintf(value, sizeof(value), "%u", static_cast<unsigned>(uxTaskGetStackHighWaterMark(GUITaskHandle))); break;
  case 14: snprintf(value, sizeof(value), "%u", static_cast<unsigned>(uxTaskGetStackHighWaterMark(MOVTaskHandle))); break;
  case 15: snprintf(value, sizeof(value), "%u", static_cast<unsigned>(uxTaskGetStackHighWaterMark(PIDTaskHandle))); break;
  case 16: {
    extern bool hs02GetFactoryTipCal(uint32_t &, uint32_t &, uint32_t &);
    uint32_t a140, a240, a340;
    hs02GetFactoryTipCal(a140, a240, a340);
    snprintf(value, sizeof(value), "%u " HS02_SYM_DOT " %u " HS02_SYM_DOT " %u", static_cast<unsigned>(a140), static_cast<unsigned>(a240), static_cast<unsigned>(a340));
  } break;
  default:
    break;
  }
  Display::drawSpaceGrotesk(value, 11, 46, 19, 1);
}
#endif
