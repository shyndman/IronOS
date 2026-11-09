
#include "Buttons.hpp"
#include "OperatingModes.h"
#include "ui_drawing.hpp"

bool showExitMenuTransition = false;

OperatingMode handleHomeButtons(const ButtonState buttons, guiContext *cxt) {
  if (buttons != BUTTON_NONE && cxt->scratch_state.state1 == 0) {
    return OperatingMode::HomeScreen; // Ignore button press
  } else {
    cxt->scratch_state.state1 = 1;
  }
#if defined(LCD_160x80)
  ButtonState btn = buttons;
  if (Display::getRotation()) {
    if (btn == BUTTON_F_SHORT) {
      btn = BUTTON_B_SHORT;
    } else if (btn == BUTTON_F_LONG) {
      btn = BUTTON_B_LONG;
    } else if (btn == BUTTON_B_SHORT) {
      btn = BUTTON_F_SHORT;
    } else if (btn == BUTTON_B_LONG) {
      btn = BUTTON_F_LONG;
    }
  }
  switch (btn) {
  case BUTTON_F_SHORT:
    if (!isTipDisconnected()) {
      cxt->transitionMode = TransitionAnimation::None;
      return OperatingMode::Soldering;
    }
    break;
  case BUTTON_OK_SHORT:
    cxt->transitionMode = TransitionAnimation::None;
    return OperatingMode::SettingsMenu;
  case BUTTON_OK_LONG:
    cxt->transitionMode = TransitionAnimation::None;
    return OperatingMode::DebugMenuReadout;
  case BUTTON_B_SHORT:
  case BUTTON_B_LONG:
  case BUTTON_NONE:
  case BUTTON_BOTH:
  default:
    break;
  }
  return OperatingMode::HomeScreen;
#else
  switch (buttons) {
  case BUTTON_NONE:
    // Do nothing
    break;
  case BUTTON_BOTH:
    break;

  case BUTTON_B_LONG:
    cxt->transitionMode = TransitionAnimation::Down;
    return OperatingMode::DebugMenuReadout;
    break;
  case BUTTON_F_LONG:
#ifdef PROFILE_SUPPORT
    if (!isTipDisconnected()) {
      cxt->transitionMode = TransitionAnimation::Left;
      return OperatingMode::SolderingProfile;
    } else {
      return OperatingMode::HomeScreen;
    }
#else
    cxt->transitionMode = TransitionAnimation::Left;
    return OperatingMode::TemperatureAdjust;
#endif
    break;
  case BUTTON_OK_SHORT: // Dedicated OK button enters soldering (same as front-press)
  case BUTTON_F_SHORT:
    if (!isTipDisconnected()) {
      bool detailedView   = getSettingValue(SettingsOptions::DetailedIDLE) && getSettingValue(SettingsOptions::DetailedSoldering);
      cxt->transitionMode = detailedView ? TransitionAnimation::None : TransitionAnimation::Left;
      return OperatingMode::Soldering;
    }
    break;
  case BUTTON_B_SHORT:
    cxt->transitionMode = TransitionAnimation::Right;
    return OperatingMode::SettingsMenu;
    break;
  default:
    break;
  }
  return OperatingMode::HomeScreen;
#endif
}

OperatingMode drawHomeScreen(const ButtonState buttons, guiContext *cxt) {

  currentTempTargetDegC = 0; // ensure tip is off
  getInputVoltageX10(getSettingValue(SettingsOptions::VoltageDiv), 0);
  uint32_t tipTemp = TipThermoModel::getTipInC();

#if defined(LCD_160x80)
  ui_draw_home_gauge_idle(tipTemp);
#else
  // Setup LCD Cursor location
  if (Display::getRotation()) {
    Display::setCursor(50, 0);
  } else {
    Display::setCursor(-1, 0);
  }
  if (getSettingValue(SettingsOptions::DetailedIDLE)) {
    ui_draw_homescreen_detailed(tipTemp);
  } else {
    ui_draw_homescreen_simplified(tipTemp);
  }
#endif
  return handleHomeButtons(buttons, cxt);
}
