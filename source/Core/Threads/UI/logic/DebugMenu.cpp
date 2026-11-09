#include "OperatingModes.h"
#include "ui_drawing.hpp"

OperatingMode showDebugMenu(const ButtonState buttons, guiContext *cxt) {
  ui_draw_debug_menu(cxt->scratch_state.state1);
#ifdef LCD_160x80
  const bool swap = Display::getRotation() ^ static_cast<bool>(getSettingValue(SettingsOptions::ReverseButtonSettings));
  if (buttons == BUTTON_OK_SHORT || buttons == BUTTON_OK_LONG || buttons == BUTTON_BOTH) {
    cxt->transitionMode = TransitionAnimation::Up;
    return OperatingMode::HomeScreen;
  }
  if ((!swap && (buttons == BUTTON_F_SHORT || buttons == BUTTON_F_LONG)) || (swap && (buttons == BUTTON_B_SHORT || buttons == BUTTON_B_LONG))) {
    cxt->scratch_state.state1 = (cxt->scratch_state.state1 == 0) ? 16 : (cxt->scratch_state.state1 - 1);
  } else if ((!swap && (buttons == BUTTON_B_SHORT || buttons == BUTTON_B_LONG)) || (swap && (buttons == BUTTON_F_SHORT || buttons == BUTTON_F_LONG))) {
    cxt->scratch_state.state1 = (cxt->scratch_state.state1 + 1) % 17;
  }
#else
  if (buttons == BUTTON_B_SHORT || buttons == BUTTON_OK_LONG) {
    cxt->transitionMode = TransitionAnimation::Up;
    return OperatingMode::HomeScreen;
  } else if (buttons == BUTTON_F_SHORT || buttons == BUTTON_OK_SHORT) {
    cxt->scratch_state.state1++;
#if defined(HALL_SENSOR)
    cxt->scratch_state.state1 = cxt->scratch_state.state1 % 17;
#else
    cxt->scratch_state.state1 = cxt->scratch_state.state1 % 16;
#endif
  }
#endif
  return OperatingMode::DebugMenuReadout;
}
