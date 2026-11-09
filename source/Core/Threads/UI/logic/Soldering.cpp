
#include "OperatingModes.h"
#include "SolderingCommon.h"
#include "ui_drawing.hpp"
// State 1 = button locking  (0:unlocked+released, 1:unlocked, 2:locked, 3:locked+released)
// State 2 = boost mode
// State 3 = buzzer timer

OperatingMode handleSolderingButtons(const ButtonState buttons, guiContext *cxt) {
#if defined(LCD_160x80)
  if (buttons == BUTTON_OK_SHORT) {
    cxt->transitionMode = TransitionAnimation::None;
    return OperatingMode::SettingsMenu;
  }
  if (buttons == BUTTON_OK_LONG) {
    cxt->transitionMode = TransitionAnimation::None;
    return OperatingMode::DebugMenuReadout;
  }
#endif
  if (cxt->scratch_state.state1 >= 2) {
    if (buttons == BUTTON_BOTH_LONG) {
      if (cxt->scratch_state.state1 == 3) {
        // Unlocking
        if (warnUser(translatedString(Tr->UnlockingKeysString), buttons)) {
          cxt->scratch_state.state1 = 1;
          cxt->scratch_state.state7 = 0;
        }
      } else {
        warnUser(translatedString(Tr->LockingKeysString), buttons);
      }
      return OperatingMode::Soldering;
    }
    if (cxt->scratch_state.state7 != 0) {
      // show locked until timer is up
      if (xTaskGetTickCount() >= cxt->scratch_state.state7) {
        cxt->scratch_state.state7 = 0;
      } else {
        warnUser(translatedString(Tr->WarningKeysLockedString), buttons);
        return OperatingMode::Soldering;
      }
    }
    switch (buttons) {
    case BUTTON_NONE:
      cxt->scratch_state.state1 = 3;
      cxt->scratch_state.state2 = 0;
      break;
    case BUTTON_F_LONG:
      if (getSettingValue(SettingsOptions::BoostTemp) && (getSettingValue(SettingsOptions::LockingMode) == lockingMode_t::BOOST)) {
        cxt->scratch_state.state2 = 1;
        break;
      }
    /*Fall through*/
    default: // Set timer for and display a lock warning
      cxt->scratch_state.state7 = xTaskGetTickCount() + TICKS_SECOND;
      warnUser(translatedString(Tr->WarningKeysLockedString), buttons);
      break;
    }
    return OperatingMode::Soldering;
  }

  // otherwise we are unlocked
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
  case BUTTON_NONE:
    cxt->scratch_state.state2 = 0;
    cxt->scratch_state.state1 = 0;
    break;
  case BUTTON_B_SHORT:
  case BUTTON_B_LONG:
  case BUTTON_BOTH:
    cxt->transitionMode = TransitionAnimation::None;
    return OperatingMode::HomeScreen;
  case BUTTON_F_LONG:
    if (getSettingValue(SettingsOptions::BoostTemp)) {
      cxt->scratch_state.state2 = 1;
    }
    break;
  case BUTTON_F_SHORT:
    cxt->transitionMode = TransitionAnimation::None;
    return OperatingMode::TemperatureAdjust;
  case BUTTON_BOTH_LONG:
    if (getSettingValue(SettingsOptions::LockingMode)) {
      if (cxt->scratch_state.state1 == 0) {
        if (warnUser(translatedString(Tr->LockingKeysString), buttons)) {
          cxt->scratch_state.state1 = 2;
        }
      } else {
        warnUser(translatedString(Tr->UnlockingKeysString), buttons);
      }
    }
    break;
  default:
    break;
  }
  return OperatingMode::Soldering;
#else
  switch (buttons) {
  case BUTTON_NONE:
    cxt->scratch_state.state2 = 0;
    cxt->scratch_state.state1 = 0;
    break;
  case BUTTON_OK_LONG: // Dedicated OK long-press returns to the home screen
  /*Fall through*/
  case BUTTON_BOTH:
  /*Fall through*/
  case BUTTON_B_LONG: {
    bool detailedView   = getSettingValue(SettingsOptions::DetailedIDLE) && getSettingValue(SettingsOptions::DetailedSoldering);
    cxt->transitionMode = detailedView ? TransitionAnimation::None : TransitionAnimation::Right;
  }
    return OperatingMode::HomeScreen;
  case BUTTON_F_LONG:
    // if boost mode is enabled turn it on
    if (getSettingValue(SettingsOptions::BoostTemp)) {
      cxt->scratch_state.state2 = 1;
    }
    break;
  case BUTTON_OK_SHORT: // Dedicated OK button opens temperature adjust
  case BUTTON_F_SHORT:
  case BUTTON_B_SHORT:
    cxt->transitionMode = TransitionAnimation::Left;
    return OperatingMode::TemperatureAdjust;
  case BUTTON_BOTH_LONG:
    if (getSettingValue(SettingsOptions::LockingMode)) {
      // Lock buttons
      if (cxt->scratch_state.state1 == 0) {
        if (warnUser(translatedString(Tr->LockingKeysString), buttons)) {
          cxt->scratch_state.state1 = 2;
        }
      } else {
        // FIXME should be WarningKeysUnlockedString
        warnUser(translatedString(Tr->UnlockingKeysString), buttons);
      }
    }
    break;
  default:
    break;
  }
  return OperatingMode::Soldering;
#endif
}

OperatingMode gui_solderingMode(const ButtonState buttons, guiContext *cxt) {
  /*
   * * Soldering (gui_solderingMode)
   * -> Main loop where we draw temp, and animations
   * --> User presses buttons and they goto the temperature adjust screen
   * ---> Display the current setpoint temperature
   * ---> Use buttons to change forward and back on temperature
   * ---> Both buttons or timeout for exiting
   * --> Long hold front button to enter boost mode
   * ---> Just temporarily sets the system into the alternate temperature for
   * PID control
   * --> Long hold back button to exit
   * --> Double button to exit
   * --> Long hold double button to toggle key lock
   */

  // Update the setpoints for the temperature
  if (cxt->scratch_state.state2) {
    if (getSettingValue(SettingsOptions::TemperatureInF)) {
      currentTempTargetDegC = TipThermoModel::convertFtoC(getSettingValue(SettingsOptions::BoostTemp));
    } else {
      currentTempTargetDegC = (getSettingValue(SettingsOptions::BoostTemp));
    }
  } else {
    if (getSettingValue(SettingsOptions::TemperatureInF)) {
      currentTempTargetDegC = TipThermoModel::convertFtoC(getSettingValue(SettingsOptions::SolderingTemp));
    } else {
      currentTempTargetDegC = (getSettingValue(SettingsOptions::SolderingTemp));
    }
  }

  // Update status
  int error = currentTempTargetDegC - TipThermoModel::getTipInC();
  if (error >= -10 && error <= 10) {
    setStatusLED(LED_HOT);
  } else {
    setStatusLED(LED_HEATING);
  }

  // Draw in the screen details
#if defined(LCD_160x80)
  ui_draw_home_gauge_soldering(cxt->scratch_state.state2);
#else
  if (getSettingValue(SettingsOptions::DetailedSoldering)) {
    ui_draw_soldering_power_status(cxt->scratch_state.state2);
  } else {
    ui_draw_soldering_basic_status(cxt->scratch_state.state2);
  }
#endif

#if !defined(LCD_160x80)
  bool detailedView = getSettingValue(SettingsOptions::DetailedIDLE) && getSettingValue(SettingsOptions::DetailedSoldering);
#endif
  // Check if we should bail due to undervoltage for example
  if (checkExitSoldering()) {
#if defined(LCD_160x80)
    cxt->transitionMode = TransitionAnimation::None; // Staying within the colour gauge screen family.
#else
    cxt->transitionMode = detailedView ? TransitionAnimation::None : TransitionAnimation::Right;
#endif
    return OperatingMode::HomeScreen;
  }
#ifdef NO_SLEEP_MODE

  if (shouldShutdown()) {
    // shutdown
    currentTempTargetDegC = 0;
#if defined(LCD_160x80)
    cxt->transitionMode = TransitionAnimation::None;
#else
    cxt->transitionMode = detailedView ? TransitionAnimation::None : TransitionAnimation::Right;
#endif
    return OperatingMode::HomeScreen;
  }
#endif
  if (shouldBeSleeping()) {
    return OperatingMode::Sleeping;
  }

  if (heaterThermalRunawayCounter > 8) {
    currentTempTargetDegC       = 0; // heater control off
    heaterThermalRunawayCounter = 0;
#if defined(LCD_160x80)
    cxt->transitionMode = TransitionAnimation::None; // ThermalRunaway is a mono screen; hard cut, see above.
#else
    cxt->transitionMode = TransitionAnimation::Right;
#endif
    return OperatingMode::ThermalRunaway;
  }
  return handleSolderingButtons(buttons, cxt);
}
