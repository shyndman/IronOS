#include "BSP.h"
#include "BSP_Power.h"
#include "FreeRTOS.h"
#include "Settings.h"
#include "Setup.h"
#include "configuration.h"
#include "task.h"

static const uint8_t  kCH224KLevels[]      = {20, 15, 12, 9};
static uint8_t        CH224KLevel           = 0;
static bool           CH224KSettled         = false;
static TickType_t     CH224KRequestedTick   = 0;
static uint16_t       lastConfiguredVolts   = 0xFFFF;

bool CH224KVoltageSettled(void) { return CH224KSettled; }

void power_check() {
  const uint16_t desiredVolts = getSettingValue(SettingsOptions::QCIdealVoltage) / 10;
  if (desiredVolts != lastConfiguredVolts) {
    lastConfiguredVolts = desiredVolts;
    CH224KLevel = 3;
    for (uint8_t i = 0; i < sizeof(kCH224KLevels) / sizeof(kCH224KLevels[0]); ++i) {
      if (kCH224KLevels[i] <= desiredVolts) {
        CH224KLevel = i;
        break;
      }
    }
    setCH224KVoltage(kCH224KLevels[CH224KLevel]);
    CH224KRequestedTick = xTaskGetTickCount();
    CH224KSettled = false;
  }

  if (CH224KSettled) {
    return;
  }
  const TickType_t now = xTaskGetTickCount();
  if (now - CH224KRequestedTick < TICKS_SECOND / 2) {
    return;
  }

  const uint16_t requestedX10 = kCH224KLevels[CH224KLevel] * 10;
  const uint16_t inputX10     = getInputVoltageX10(getSettingValue(SettingsOptions::VoltageDiv), 1);
  const bool     withinLower  = inputX10 * 100 >= requestedX10 * 85;
  const bool     withinUpper  = inputX10 * 100 <= requestedX10 * 115;
  if ((withinLower && withinUpper) || (CH224KLevel == 3 && withinUpper)) {
    CH224KSettled = true;
    return;
  }
  if (CH224KLevel == 3) {
    return;
  }

  ++CH224KLevel;
  setCH224KVoltage(kCH224KLevels[CH224KLevel]);
  CH224KRequestedTick = now;
}

// The CH224K does the PD negotiation, so IronOS sees a DC input.
bool getIsPoweredByDCIN() { return true; }
