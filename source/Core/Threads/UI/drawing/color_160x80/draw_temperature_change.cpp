#include "Hs02UiAssets.hpp"
#include "ui_drawing.hpp"
#include <stdio.h>

#ifdef LCD_160x80
void ui_draw_temperature_change(void) {
  Display::setColorPalette(Hs02UI::kPaletteHomeSolder);
  const bool reversed = getSettingValue(SettingsOptions::ReverseButtonTempChangeEnabled);
  Display::drawSpaceGrotesk("Set temperature", 11, 7, 15, 0);
  Display::drawSpaceGrotesk(reversed ? "+" : HS02_SYM_MINUS, 10, 42, 19, 1);
  Display::drawSpaceGrotesk(reversed ? HS02_SYM_MINUS : "+", 137, 42, 19, 1);

  char buf[8];
  snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(getSettingValue(SettingsOptions::SolderingTemp)));
  const char   *unit   = getSettingValue(SettingsOptions::TemperatureInF) ? (HS02_SYM_DEG "F") : (HS02_SYM_DEG "C");
  const int16_t numW   = Display::measureSpaceGrotesk(buf, 30);
  const int16_t unitW  = Display::measureSpaceGrotesk(unit, 17);
  const int16_t totalW = numW + 2 + unitW;
  const int16_t startX = (160 - totalW) / 2;
  Display::drawSpaceGrotesk(buf, startX, 34, 30, 0);
  Display::drawSpaceGrotesk(unit, startX + numW + 2, 43, 17, 0);
}
#endif
