#include "Hs02UiAssets.hpp"
#include "ui_drawing.hpp"
#include <stdio.h>
#ifdef LCD_160x80
void ui_draw_warning_undervoltage(void) {
  Display::setColorPalette(Hs02UI::kPaletteMenuPower);
  Display::clearScreenColor();
  Display::drawRoundedBarColor(0, 0, 3, 80, 1);
  Display::drawAaBitmapColor(Hs02UI::kIconTriangleAlert.data, Hs02UI::kIconTriangleAlert.width, Hs02UI::kIconTriangleAlert.height, 11, 8, 1);
  Display::drawSpaceGrotesk("Undervoltage", 35, 7, 15, 0);
  const uint32_t voltage = getInputVoltageX10(getSettingValue(SettingsOptions::VoltageDiv), 0);
  char buf[24];
  snprintf(buf, sizeof(buf), "Input %u.%u V", static_cast<unsigned>(voltage / 10), static_cast<unsigned>(voltage % 10));
  Display::drawSpaceGrotesk(buf, 11, 46, 19, 1);
  Display::refresh();
  GUIDelay();
  waitForButtonPress();
}
#endif
