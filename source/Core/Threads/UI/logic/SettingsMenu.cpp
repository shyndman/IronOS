#include "Display.hpp"
#include "OperatingModes.h"
#include "ScrollMessage.hpp"
#ifdef LCD_160x80
#include "Hs02UiAssets.hpp"
#define HELP_TEXT_TIMEOUT_TICKS (TICKS_SECOND * 4)
#else
#define HELP_TEXT_TIMEOUT_TICKS (TICKS_SECOND * 3)
#endif
/*
 * The settings menu is the most complex bit of GUI code we have
 * The menu consists of a two tier menu
 * Main menu -> Categories
 * Secondary menu -> Settings
 *
 * For each entry in the menu
 */

/**
 * Prints two small lines (or one line for CJK) of short description for
 * setting items and prepares cursor after it.
 * @param settingsItemIndex Index of the setting item.
 * @param cursorCharPosition Custom cursor char position to set after printing
 * description.
 */
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

#ifdef LCD_160x80
static const uint16_t *hs02MenuPalette(uint16_t mainEntry) {
  static const uint16_t *const kPalettes[5] = {
      Hs02UI::kPaletteMenuPower,
      Hs02UI::kPaletteMenuSolder,
      Hs02UI::kPaletteMenuSleep,
      Hs02UI::kPaletteMenuUI,
      Hs02UI::kPaletteMenuAdvanced,
  };
  return kPalettes[mainEntry < 5 ? mainEntry : 0];
}
#endif

// Render a menu, based on the position given
// This will either draw the menu item, or the help text depending on how long its been since button press
void render_menu(const menuitem *item, guiContext *cxt) {
#ifdef LCD_160x80
  const uint16_t mainEntry = cxt->scratch_state.state1;
  const uint16_t subEntry  = cxt->scratch_state.state2;
  if (subEntry == 0) {
    item->draw();
    return;
  }

  Display::setColorPalette(hs02MenuPalette(mainEntry));
  Display::drawRoundedBarColor(0, 0, 3, 80, 1);

  if ((xTaskGetTickCount() - lastButtonTime < HELP_TEXT_TIMEOUT_TICKS) || item->description == 0) {
    bool hasNL = false;
    if (item->shortDescriptionSize > 0) {
      const char *label = hs02SettingLabel(item->shortDescriptionIndex);
      for (const char *p = label; *p; ++p) {
        if (*p == '\n') {
          hasNL = true;
          break;
        }
      }
      Display::drawSpaceGrotesk(label, 11, hasNL ? 5 : 7, 15, 0);
    }
    hs02SetTwoLineSettingLayout(hasNL);
    const SettingsItemIndex idx        = item->shortDescriptionIndex;
    const bool              isCheckbox = (idx == SettingsItemIndex::CooldownBlink || idx == SettingsItemIndex::ReverseButtonTempChange ||
                             idx == SettingsItemIndex::ReverseButtonSettings || idx == SettingsItemIndex::CalibrateCJC);
    const int16_t           chevronY   = hasNL ? 52 : 50;
    if (idx == SettingsItemIndex::VoltageCalibration) {
      Display::drawSpaceGrotesk("Calibrate", 11, hasNL ? 48 : 46, 19, 1);
      Display::drawAaBitmapColor(Hs02UI::kIconChevronRight.data, Hs02UI::kIconChevronRight.width, Hs02UI::kIconChevronRight.height, 137, chevronY, 1);
    } else if (idx == SettingsItemIndex::SettingsReset) {
      Display::drawSpaceGrotesk("Reset", 11, hasNL ? 48 : 46, 19, 1);
      Display::drawAaBitmapColor(Hs02UI::kIconChevronRight.data, Hs02UI::kIconChevronRight.width, Hs02UI::kIconChevronRight.height, 137, chevronY, 1);
    } else if (isCheckbox) {
      item->draw();
    } else {
      Display::drawAaBitmapColor(Hs02UI::kIconChevronLeft.data, Hs02UI::kIconChevronLeft.width, Hs02UI::kIconChevronLeft.height, 10, chevronY, 1);
      item->draw();
      Display::drawAaBitmapColor(Hs02UI::kIconChevronRight.data, Hs02UI::kIconChevronRight.width, Hs02UI::kIconChevronRight.height, 137, chevronY, 1);
    }
  } else {
    uint16_t *isRenderingHelp = &(cxt->scratch_state.state6);
    *isRenderingHelp          = 1;
    const char      *helpText = hs02SettingHelp(item->shortDescriptionIndex);
    const uint16_t   msgWidth = Display::measureSpaceGrotesk(helpText, 19);
    const TickType_t tickOff  = (xTaskGetTickCount() - lastButtonTime) - HELP_TEXT_TIMEOUT_TICKS;
    constexpr TickType_t stepDiv = TICKS_100MS / 5;
    const uint32_t   pausePx  = ((TICKS_SECOND * 4 / 5) * 7u) / (5u * stepDiv);
    const uint32_t   gapPx    = ((TICKS_SECOND / 2) * 7u) / (5u * stepDiv);
    const uint32_t   outPx    = msgWidth + 8u;
    const uint32_t   inPx     = 149u;
    const uint32_t   cyclePx  = pausePx + outPx + gapPx + inPx;
    const uint32_t   posPx    = ((tickOff * 7u) / (5u * stepDiv)) % cyclePx;
    int16_t          drawX    = 11;
    if (posPx < pausePx) {
      drawX = 11;
    } else if (posPx < pausePx + outPx) {
      drawX = 11 - static_cast<int16_t>(posPx - pausePx);
    } else if (posPx < pausePx + outPx + gapPx) {
      drawX = LCD_WIDTH;
    } else {
      drawX = LCD_WIDTH - static_cast<int16_t>(posPx - (pausePx + outPx + gapPx));
    }
    if (drawX < LCD_WIDTH) {
      Display::drawSpaceGrotesk(helpText, drawX, 28, 19, 0, 4, LCD_WIDTH);
    }
  }
#else
  // If recent interaction or not help text draw the entry
  if ((xTaskGetTickCount() - lastButtonTime < HELP_TEXT_TIMEOUT_TICKS) || item->description == 0) {

    if (item->shortDescriptionSize > 0) {
      printShortDescription(item->shortDescriptionIndex, item->shortDescriptionSize);
    }
    item->draw();
  } else {

    uint16_t *isRenderingHelp = &(cxt->scratch_state.state6);
    *isRenderingHelp          = 1;
    // Draw description
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
  // Render out the current settings menu
  // State 1 -> Root menu
  // State 2 -> Sub entry
  // Draw main entry if sub-entry is 0, otherwise draw sub-entry

  uint16_t *mainEntry              = &(cxt->scratch_state.state1);
  uint16_t *subEntry               = &(cxt->scratch_state.state2);
  uint32_t *autoRepeatAcceleration = &(cxt->scratch_state.state3);
  uint32_t *autoRepeatTimer        = &(cxt->scratch_state.state4);
  uint16_t *currentMenuLength      = &(cxt->scratch_state.state5);
  uint16_t *isRenderingHelp        = &(cxt->scratch_state.state6);

  const menuitem *currentMenu;
  // Draw the currently on screen item
  uint16_t currentScreen;
  if (*subEntry == 0) {
    // Drawing main menu
    currentMenu   = rootSettingsMenu;
    currentScreen = *mainEntry;
  } else {
    // Drawing sub menu
    currentMenu   = subSettingsMenus[*mainEntry];
    currentScreen = (*subEntry) - 1;
  }
  render_menu(&(currentMenu[currentScreen]), cxt);

  // Update the cached menu length if unknown
  if (*currentMenuLength == 0) {
    // We walk the current menu to find the length
    *currentMenuLength = getMenuLength(currentMenu, 128 /* Max length of any menu*/);
  }

#ifndef LCD_160x80
  if (*isRenderingHelp == 0) {
    //  Draw scroll

    // Get virtual pos by counting entries from start to _here_
    uint16_t currentVirtualPosition = getMenuLength(currentMenu, currentScreen + 1);
    if (currentVirtualPosition > 0) {
      currentVirtualPosition--;
    }

    // The height of the indicator is screen res height / total menu entries
    uint8_t indicatorHeight = DISPLAY_HEIGHT / *currentMenuLength;
    if (indicatorHeight == 0) {
      indicatorHeight = 1; // always at least 1 pixel
    }

    uint16_t position = (DISPLAY_HEIGHT * (uint16_t)currentVirtualPosition) / *currentMenuLength;

    bool showScrollbar = true;

    // Store if its the last option for this setting
    bool isLastOptionForSetting = false;
    if ((int)currentMenu[currentScreen].autoSettingOption < (int)SettingsOptions::SettingsOptionsLength) {
      isLastOptionForSetting = isLastSettingValue(currentMenu[currentScreen].autoSettingOption);
    }

    // Last settings menu entry, reset scroll show back so it flashes
    if (isLastOptionForSetting) {
      showScrollbar = false;
    }

    // Or Flash it
    showScrollbar |= (xTaskGetTickCount() % (TICKS_SECOND / 4) < (TICKS_SECOND / 8));

    if (showScrollbar) {
      Display::drawScrollIndicator((uint8_t)position, indicatorHeight);
    }
  }
#endif

  // Now handle user button input
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
