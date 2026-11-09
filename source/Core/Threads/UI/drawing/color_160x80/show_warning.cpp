#include "Buttons.hpp"
#include "Hs02UiAssets.hpp"
#include "OperatingModeUtilities.h"
#include "OperatingModes.h"
#include "ScrollMessage.hpp"
#include "ui_drawing.hpp"
#include <string.h>
#ifdef LCD_160x80
bool warnUser(const char *warning, const ButtonState buttons) {
  const uint16_t *palette = Hs02UI::kPaletteMenuPower;
  const Hs02UI::Icon2bpp *icon = &Hs02UI::kIconTriangleAlert;
  const char *title = "Alert";
  const char *detail = "";
  if (warning == translatedString(Tr->WarningThermalRunaway)) {
    palette = Hs02UI::kPaletteAlertDanger; title = "Thermal runaway"; detail = "Heater stopped";
  } else if (warning == translatedString(Tr->WarningTipShorted)) {
    palette = Hs02UI::kPaletteAlertDanger; title = "Tip shorted"; detail = "Heater stopped";
  } else if (warning == translatedString(Tr->LockingKeysString) || warning == translatedString(Tr->WarningKeysLockedString)) {
    palette = Hs02UI::kPaletteMenuSolder; icon = &Hs02UI::kIconLock; title = "Buttons"; detail = "Locked";
  } else if (warning == translatedString(Tr->UnlockingKeysString)) {
    palette = Hs02UI::kPaletteMenuSolder; icon = &Hs02UI::kIconLockOpen; title = "Buttons"; detail = "Unlocked";
  } else if (warning == translatedString(Tr->CalibrationDone)) {
    palette = Hs02UI::kPaletteMenuAdvanced; icon = &Hs02UI::kIconCircleCheck; title = "Calibration"; detail = "Saved";
  } else if (warning == translatedString(Tr->ResetOKMessage)) {
    palette = Hs02UI::kPaletteMenuAdvanced; icon = &Hs02UI::kIconCircleCheck; title = "Settings"; detail = "Reset OK";
  } else if (warning == translatedString(Tr->SettingsResetMessage)) {
    palette = Hs02UI::kPaletteMenuAdvanced; title = "Settings"; detail = "Changed";
  } else if (warning == translatedString(Tr->NoAccelerometerMessage)) {
    palette = Hs02UI::kPaletteMenuPower; title = "Accelerometer"; detail = "Not detected";
  }
  Display::setColorPalette(palette);
  Display::clearScreenColor();
  Display::drawRoundedBarColor(0, 0, 3, 80, 1);
  Display::drawAaBitmapColor(icon->data, icon->width, icon->height, 11, 8, 1);
  Display::drawSpaceGrotesk(title, 35, 7, 15, 0);
  Display::drawSpaceGrotesk(detail, 11, 46, 19, 1);
  if ((xTaskGetTickCount() - lastButtonTime) > TICKS_SECOND * 5) return true;
  return buttons != BUTTON_NONE;
}
#endif
