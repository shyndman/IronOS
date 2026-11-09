#include "Hs02UiAssets.hpp"
#include "ui_drawing.hpp"
#ifdef LCD_160x80
void ui_draw_cjc_sampling(const uint8_t num_dots) {
  Display::setColorPalette(Hs02UI::kPaletteMenuAdvanced);
  Display::drawRoundedBarColor(0, 0, 3, 80, 1);
  Display::drawSpaceGrotesk("Calibrating CJC", 11, 7, 15, 0);
  Display::drawRoundedBarColor(11, 52, 138, 1, 1);
  Display::drawRoundedBarColor(11, 59, 138, 1, 1);
  Display::drawRoundedBarColor(11, 52, 1, 8, 1);
  Display::drawRoundedBarColor(148, 52, 1, 8, 1);
  uint16_t fillW = (static_cast<uint16_t>(num_dots) * 138) / 4;
  if (fillW < 8) fillW = 8;
  if (fillW > 138) fillW = 138;
  Display::drawRoundedBarColor(11, 52, static_cast<uint8_t>(fillW), 8, 1);
}
#endif
