#include "Display.hpp"
#include "OperatingModes.h"
#include "ScrollMessage.hpp"
#ifdef LCD_160x80
#include "Hs02UiAssets.hpp"
#include "HudTelemetry.hpp"
#define HELP_TEXT_TIMEOUT_TICKS (TICKS_SECOND * 4)
#else
#define HELP_TEXT_TIMEOUT_TICKS (TICKS_SECOND * 3)
#endif

#ifdef LCD_160x80
uint16_t getMenuLength(const menuitem *menu, const uint16_t stop);
static void drawSettingTitle(const char *label, int16_t x, int16_t y, uint16_t color, int16_t clipRight) {
  Display::drawSpaceGroteskRgb(label, x, y, 15, color, x, clipRight);
}

static uint8_t makeOneLineSettingName(SettingsItemIndex idx, char *buf, uint8_t cap) {
  const char *label = hs02SettingLabel(idx);
  if (idx == SettingsItemIndex::LockingMode) {
    label = "Button lock";
  }

  uint8_t n = 0;
  while (*label && n + 1u < cap) {
    buf[n++] = (*label == '\n') ? ' ' : *label;
    ++label;
  }
  buf[n] = '\0';
  return n;
}

static void renderHs02Help(const menuitem *item, guiContext *cxt, const ChaosHud::CategoryTheme &theme) {
  cxt->scratch_state.state6 = 1;
  char title[32];
  makeOneLineSettingName(item->shortDescriptionIndex, title, sizeof(title));
  char value[32];
  ChaosHud::formatSettingSummaryPill(item->shortDescriptionIndex, value, sizeof(value));
  const int16_t pillX = ChaosHud::drawPillRight(152, 5, 19, theme.dim, ChaosHud::Color::kNone, value, theme.fg);
  drawSettingTitle(title, 10, 5, ChaosHud::Color::kMuted, static_cast<int16_t>(pillX - 3));

  const char *help = hs02SettingHelp(item->shortDescriptionIndex);
  constexpr uint16_t viewW = 136;
  constexpr uint16_t thumbW = 38;
  constexpr uint16_t travel = 142u - thumbW;
  const uint16_t msgW = Display::measureSpaceGrotesk(help, 15);
  const TickType_t elapsed = (xTaskGetTickCount() - lastButtonTime) - HELP_TEXT_TIMEOUT_TICKS;
  const uint32_t maxScroll = (msgW > viewW) ? static_cast<uint32_t>(msgW - viewW + 12u) : 0u;
  constexpr uint32_t kPauseSteps = 25u;
  const uint32_t cycle = kPauseSteps + maxScroll + kPauseSteps;
  const uint32_t step = (elapsed / (TICKS_100MS / 3u)) % (cycle ? cycle : 1u);
  const uint32_t phase = (step < kPauseSteps) ? 0u : ((step < kPauseSteps + maxScroll) ? (step - kPauseSteps) : maxScroll);
  const int16_t textX = static_cast<int16_t>(16 - static_cast<int32_t>(phase));
  const int16_t thumbX = static_cast<int16_t>(10u + ((maxScroll == 0u) ? 0u : (phase * travel) / maxScroll));
  Display::drawBoxRgb(10, 30, 142, 2, ChaosHud::Color::kTrack);
  Display::drawBoxRgb(thumbX, 30, thumbW, 2, theme.fg);
  ChaosHud::drawBox(10, 38, 144, 32, ChaosHud::Color::kCell, ChaosHud::Color::kBorder, 4);
  Display::drawSpaceGroteskRgb(help, textX, 44, 15, ChaosHud::Color::kInk, 15, 149);
}

static void renderHs02Setting(const menuitem *item, guiContext *cxt) {
  const uint16_t mainEntry = cxt->scratch_state.state1;
  const uint16_t subEntry = cxt->scratch_state.state2;
  Display::setColorMode(true);
  Display::clearScreenColor();
  if (subEntry == 0u) {
    item->draw();
    return;
  }

  const uint8_t menuLen = static_cast<uint8_t>(getMenuLength(subSettingsMenus[mainEntry], 128));
  const uint8_t visIdx = static_cast<uint8_t>(getMenuLength(subSettingsMenus[mainEntry], subEntry) - 1u);
  const ChaosHud::CategoryTheme &theme = ChaosHud::categoryTheme(mainEntry);
  const uint16_t active = (item->shortDescriptionIndex == SettingsItemIndex::SettingsReset) ? ChaosHud::Color::kRedline : theme.fg;
  ChaosHud::drawSegmentedRail(menuLen, visIdx, active, theme.dim);

  if ((xTaskGetTickCount() - lastButtonTime < HELP_TEXT_TIMEOUT_TICKS) || item->description == 0u) {
    const SettingsItemIndex idx   = item->shortDescriptionIndex;
    const char             *label = (idx == SettingsItemIndex::LockingMode) ? "Button lock" : hs02SettingLabel(idx);
    bool                    hasNL = false;
    for (const char *p = label; *p; ++p) {
      if (*p == '\n') {
        hasNL = true;
        break;
      }
    }
    drawSettingTitle(label, 10, hasNL ? 4 : 5, ChaosHud::Color::kInk, 153);
    ChaosHud::drawCustomSettingScreen(idx);
  } else {
    renderHs02Help(item, cxt, theme);
  }
}
#endif // LCD_160x80

static void printShortDescription(SettingsItemIndex settingsItemIndex, uint16_t cursorCharPosition) {
#ifdef LCD_160x80
  (void)settingsItemIndex;
  (void)cursorCharPosition;
#else
  // print short description (default single line, explicit double line)
  uint8_t shortDescIndex = static_cast<uint8_t>(settingsItemIndex);
  Display::printWholeScreen(translatedString(Tr->SettingsShortNames[shortDescIndex]));

  // prepare cursor for value
  // make room for scroll indicator
  Display::setCursor(cursorCharPosition * FONT_LARGE_WIDTH - 2, 0);
#endif
}


void render_menu(const menuitem *item, guiContext *cxt) {
#ifdef LCD_160x80
  renderHs02Setting(item, cxt);
#else
  if ((xTaskGetTickCount() - lastButtonTime < HELP_TEXT_TIMEOUT_TICKS) || item->description == 0) {

    if (item->shortDescriptionSize > 0) {
      printShortDescription(item->shortDescriptionIndex, item->shortDescriptionSize);
    }
    item->draw();
  } else {

    uint16_t *isRenderingHelp = &(cxt->scratch_state.state6);
    *isRenderingHelp          = 1;
    const char *description = translatedString(Tr->SettingsDescriptions[item->description - 1]);
    drawScrollingText(description, (xTaskGetTickCount() - lastButtonTime) - HELP_TEXT_TIMEOUT_TICKS);
  }
#endif
}

uint16_t getMenuLength(const menuitem *menu, const uint16_t stop) {
  // walk this menu to find the length
  uint16_t counter = 0;
  for (uint16_t pos = 0; pos < stop; pos++) {
    // End of list
    if (menu[pos].draw == nullptr) {
      return counter;
    }
    // Otherwise increment for each visible item (null == always, or if not check function)
    if (menu[pos].isVisible == nullptr || menu[pos].isVisible()) {
      counter++;
    }
  }
  return counter;
}

OperatingMode moveToNextEntry(guiContext *cxt) {
  uint16_t *mainEntry         = &(cxt->scratch_state.state1);
  uint16_t *subEntry          = &(cxt->scratch_state.state2);
  uint16_t *currentMenuLength = &(cxt->scratch_state.state5);
  uint16_t *isRenderingHelp   = &(cxt->scratch_state.state6);

  if (*isRenderingHelp) {
    *isRenderingHelp = 0;
  } else {
    *currentMenuLength = 0; // Reset menu length
    // Scroll down
    // We can increment freely _once_
    cxt->transitionMode = TransitionAnimation::Down;
    if (*subEntry == 0) {
      (*mainEntry) += 1;

      if (rootSettingsMenu[*mainEntry].draw == nullptr) {
        // We are off the end of the menu now
        saveSettings();
        cxt->transitionMode = TransitionAnimation::Left;
        return OperatingMode::HomeScreen;
      }
      // Check if visible
      if (rootSettingsMenu[*mainEntry].isVisible != nullptr && !rootSettingsMenu[*mainEntry].isVisible()) {
        // We need to move on as this one isn't visible
        return moveToNextEntry(cxt);
      }
    } else {
      (*subEntry) += 1;

      // If the new entry is null, we need to exit
      if (subSettingsMenus[*mainEntry][(*subEntry) - 1].draw == nullptr) {
        (*subEntry)         = 0; // Reset back to the main menu
        cxt->transitionMode = TransitionAnimation::Left;
        // Have to break early to avoid the below check underflowing
        return OperatingMode::SettingsMenu;
      }
      // Check if visible
      if (subSettingsMenus[*mainEntry][(*subEntry) - 1].isVisible != nullptr && !subSettingsMenus[*mainEntry][(*subEntry) - 1].isVisible()) {
        // We need to move on as this one isn't visible
        return moveToNextEntry(cxt);
      }
    }
  }
  return OperatingMode::SettingsMenu;
}

OperatingMode gui_SettingsMenu(const ButtonState buttons, guiContext *cxt) {

  uint16_t *mainEntry              = &(cxt->scratch_state.state1);
  uint16_t *subEntry               = &(cxt->scratch_state.state2);
  uint32_t *autoRepeatAcceleration = &(cxt->scratch_state.state3);
  uint32_t *autoRepeatTimer        = &(cxt->scratch_state.state4);
  uint16_t *currentMenuLength      = &(cxt->scratch_state.state5);
  uint16_t *isRenderingHelp        = &(cxt->scratch_state.state6);

  const menuitem *currentMenu;
  uint16_t currentScreen;
  if (*subEntry == 0) {
    currentMenu   = rootSettingsMenu;
    currentScreen = *mainEntry;
  } else {
    currentMenu   = subSettingsMenus[*mainEntry];
    currentScreen = (*subEntry) - 1;
  }
  render_menu(&(currentMenu[currentScreen]), cxt);

  if (*currentMenuLength == 0) {
    *currentMenuLength = getMenuLength(currentMenu, 128 /* Max length of any menu*/);
  }

#ifndef LCD_160x80
  if (*isRenderingHelp == 0) {
    uint16_t currentVirtualPosition = getMenuLength(currentMenu, currentScreen + 1);
    if (currentVirtualPosition > 0) {
      currentVirtualPosition--;
    }

    uint8_t indicatorHeight = DISPLAY_HEIGHT / *currentMenuLength;
    if (indicatorHeight == 0) {
      indicatorHeight = 1; // always at least 1 pixel
    }

    uint16_t position = (DISPLAY_HEIGHT * (uint16_t)currentVirtualPosition) / *currentMenuLength;

    bool showScrollbar = true;

    bool isLastOptionForSetting = false;
    if ((int)currentMenu[currentScreen].autoSettingOption < (int)SettingsOptions::SettingsOptionsLength) {
      isLastOptionForSetting = isLastSettingValue(currentMenu[currentScreen].autoSettingOption);
    }

    if (isLastOptionForSetting) {
      showScrollbar = false;
    }

    showScrollbar |= (xTaskGetTickCount() % (TICKS_SECOND / 4) < (TICKS_SECOND / 8));

    if (showScrollbar) {
      Display::drawScrollIndicator((uint8_t)position, indicatorHeight);
    }
  }
#endif

  auto callIncrementHandler = [&]() {
    if (currentMenu[currentScreen].incrementHandler != nullptr) {
      currentMenu[currentScreen].incrementHandler();
    } else if ((int)currentMenu[currentScreen].autoSettingOption < (int)SettingsOptions::SettingsOptionsLength) {
      nextSettingValue(currentMenu[currentScreen].autoSettingOption);
    }
    return false;
  };

#ifdef LCD_160x80
  auto callDecrementHandler = [&]() {
    const SettingsOptions opt = currentMenu[currentScreen].autoSettingOption;
    if ((int)opt < (int)SettingsOptions::SettingsOptionsLength) {
      if (currentMenu[currentScreen].incrementHandler != nullptr) {
        if (opt == SettingsOptions::BoostTemp || opt == SettingsOptions::SleepTemp || opt == SettingsOptions::OrientationMode) {
          stepPrevCustomSetting(opt);
        } else {
          currentMenu[currentScreen].incrementHandler();
        }
      } else {
        prevSettingValue(opt);
      }
    }
  };

  auto stepRootMenu = [&](int8_t dir) {
    uint16_t total = 0;
    while (rootSettingsMenu[total].draw != nullptr) {
      total++;
    }
    if (total == 0) {
      return;
    }
    for (uint16_t tries = 0; tries < total; ++tries) {
      if (dir > 0) {
        *mainEntry = (*mainEntry + 1) % total;
      } else {
        *mainEntry = (*mainEntry == 0) ? (total - 1) : (*mainEntry - 1);
      }
      if (rootSettingsMenu[*mainEntry].isVisible == nullptr || rootSettingsMenu[*mainEntry].isVisible()) {
        break;
      }
    }
    *currentMenuLength = 0;
  };

  const bool swap = Display::getRotation() ^ static_cast<bool>(getSettingValue(SettingsOptions::ReverseButtonSettings));
  ButtonState btn = buttons;
  if (swap) {
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
    (*autoRepeatAcceleration) = 0;
    (*autoRepeatTimer)        = 0;
    break;
  case BUTTON_BOTH:
  case BUTTON_OK_LONG:
    if (*subEntry == 0) {
      saveSettings();
      return OperatingMode::HomeScreen;
    } else {
      *subEntry          = 0;
      *isRenderingHelp   = 0;
      *currentMenuLength = 0;
      return OperatingMode::SettingsMenu;
    }
  case BUTTON_OK_SHORT:
    if (*isRenderingHelp) {
      *isRenderingHelp = 0;
    } else if (*subEntry == 0) {
      (*subEntry)        = 1;
      *currentMenuLength = 0;
    } else {
      return moveToNextEntry(cxt);
    }
    break;
  case BUTTON_F_LONG:
    if (xTaskGetTickCount() + (*autoRepeatAcceleration) > (*autoRepeatTimer) + PRESS_ACCEL_INTERVAL_MAX) {
      (*autoRepeatTimer) = xTaskGetTickCount();
      (*autoRepeatAcceleration) += PRESS_ACCEL_STEP;
      if (*isRenderingHelp) {
        *isRenderingHelp = 0;
      } else if (*subEntry == 0) {
        stepRootMenu(-1);
      } else {
        callDecrementHandler();
        *currentMenuLength = 0;
      }
    }
    break;
  case BUTTON_F_SHORT:
    if (*isRenderingHelp) {
      *isRenderingHelp = 0;
    } else if (*subEntry == 0) {
      stepRootMenu(-1);
    } else {
      callDecrementHandler();
      *currentMenuLength = 0;
    }
    break;
  case BUTTON_B_LONG:
    if (xTaskGetTickCount() + (*autoRepeatAcceleration) > (*autoRepeatTimer) + PRESS_ACCEL_INTERVAL_MAX) {
      (*autoRepeatTimer) = xTaskGetTickCount();
      (*autoRepeatAcceleration) += PRESS_ACCEL_STEP;
      if (*isRenderingHelp) {
        *isRenderingHelp = 0;
      } else if (*subEntry == 0) {
        stepRootMenu(1);
      } else {
        callIncrementHandler();
        *currentMenuLength = 0;
      }
    }
    break;
  case BUTTON_B_SHORT:
    if (*isRenderingHelp) {
      *isRenderingHelp = 0;
    } else if (*subEntry == 0) {
      stepRootMenu(1);
    } else {
      callIncrementHandler();
      *currentMenuLength = 0;
    }
    break;
  default:
    break;
  }

  if ((PRESS_ACCEL_INTERVAL_MAX - (*autoRepeatAcceleration)) < PRESS_ACCEL_INTERVAL_MIN) {
    (*autoRepeatAcceleration) = PRESS_ACCEL_INTERVAL_MAX - PRESS_ACCEL_INTERVAL_MIN;
  }
  return OperatingMode::SettingsMenu;
#else
  // Modify a button value before processing a key press if setting to swap buttons is enabled
  bool    swapButtonSettings = getSettingValue(SettingsOptions::ReverseButtonSettings);
  uint8_t buttonPress;
  switch (buttons) {
  case BUTTON_F_LONG:
    buttonPress = swapButtonSettings ? BUTTON_B_LONG : BUTTON_F_LONG;
    break;
  case BUTTON_F_SHORT:
    buttonPress = swapButtonSettings ? BUTTON_B_SHORT : BUTTON_F_SHORT;
    break;
  case BUTTON_B_LONG:
    buttonPress = swapButtonSettings ? BUTTON_F_LONG : BUTTON_B_LONG;
    break;
  case BUTTON_B_SHORT:
    buttonPress = swapButtonSettings ? BUTTON_F_SHORT : BUTTON_B_SHORT;
    break;
  case BUTTON_OK_SHORT:
    buttonPress = BUTTON_F_SHORT; // Dedicated OK button selects/enters, ignoring button swap
    break;
  case BUTTON_OK_LONG:
    buttonPress = BUTTON_BOTH; // OK long-press goes back one level (submenu -> root -> home)
    break;
  default:
    buttonPress = buttons;
    break;
  }

  OperatingMode newMode = OperatingMode::SettingsMenu;
  switch (buttonPress) {
  case BUTTON_NONE:
    (*autoRepeatAcceleration) = 0; // reset acceleration
    (*autoRepeatTimer)        = 0; // reset acceleration
    break;
  case BUTTON_BOTH:
    if (*subEntry == 0) {
      saveSettings();
      cxt->transitionMode = TransitionAnimation::Left;
      return OperatingMode::HomeScreen;
    } else {
      cxt->transitionMode = TransitionAnimation::Left;
      *subEntry           = 0;
      return OperatingMode::SettingsMenu;
    }
    break;
  case BUTTON_F_LONG:
    if (xTaskGetTickCount() + (*autoRepeatAcceleration) > (*autoRepeatTimer) + PRESS_ACCEL_INTERVAL_MAX) {
      callIncrementHandler();
      // Update the check for if its the last version
      bool isLastOptionForSetting = false;
      if ((int)currentMenu[currentScreen].autoSettingOption < (int)SettingsOptions::SettingsOptionsLength) {
        isLastOptionForSetting = isLastSettingValue(currentMenu[currentScreen].autoSettingOption);
      }

      if (isLastOptionForSetting) {
        (*autoRepeatTimer) = TICKS_SECOND * 2;
      } else {
        (*autoRepeatTimer) = 0;
      }
      (*autoRepeatTimer) += xTaskGetTickCount();
      (*autoRepeatAcceleration) += PRESS_ACCEL_STEP;
      *currentMenuLength = 0; // Reset incase menu visible changes
    }
    break;
  case BUTTON_F_SHORT:
    // Increment setting
    if (*isRenderingHelp) {
      *isRenderingHelp = 0;
    } else {
      *currentMenuLength = 0; // Reset incase menu visible changes
      if (*subEntry == 0) {
        // In a root menu, if its null handler we enter the menu
        if (currentMenu[currentScreen].incrementHandler != nullptr) {
          currentMenu[currentScreen].incrementHandler();
        } else {
          (*subEntry) += 1;
          cxt->transitionMode = TransitionAnimation::Right;
        }
      } else {
        callIncrementHandler();
      }
    }
    break;
  case BUTTON_B_LONG:
    if (xTaskGetTickCount() + (*autoRepeatAcceleration) > (*autoRepeatTimer) + PRESS_ACCEL_INTERVAL_MAX) {
      (*autoRepeatTimer) = xTaskGetTickCount();
      (*autoRepeatAcceleration) += PRESS_ACCEL_STEP;
    } else {
      break;
    }
    /* Fall through*/
  case BUTTON_B_SHORT:
    // Increment menu item
    newMode = moveToNextEntry(cxt);
    break;
  default:
    break;
  }

  if ((PRESS_ACCEL_INTERVAL_MAX - (*autoRepeatAcceleration)) < PRESS_ACCEL_INTERVAL_MIN) {
    (*autoRepeatAcceleration) = PRESS_ACCEL_INTERVAL_MAX - PRESS_ACCEL_INTERVAL_MIN;
  }

  // Otherwise we stay put for next render iteration
  return newMode;
#endif
}
