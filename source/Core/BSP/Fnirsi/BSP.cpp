// BSP mapping functions

#include "BSP.h"
#include "n32l40x_gpio.h"
#include "n32l40x_wwdg.h"
// #include "BootLogo.h"
// #include "I2C_Wrapper.hpp"
#include "Buttons.hpp"
#include "FnirsiBootLogo.h"
#include "FreeRTOS.h"
#include "Pins.h"
#include "Settings.h"
#include "Setup.h"
#include "TipThermoModel.h"
#include "history.hpp"
#include "task.h"
// n32l40x.h defines LCD as a peripheral pointer macro; free the name for the LCD class.
#undef LCD
#include "LCD.hpp"
// #include "USBPD.h"
// #include "configuration.h"
// #include "history.hpp"
// #include "main.hpp"
// #include <IRQ.h>

volatile uint16_t PWMSafetyTimer = 0;
volatile uint8_t  pendingPWM     = 0;

// Our main PWM timer runs at 20Hz, 400 ticks per cycle (8kHz tick)
// ADC runs at 8MHz and needs less than one timer tick to perform all conversions
const uint16_t        powerPWM         = 395;                                        // pulse when TIM1 output power is enabled
static const uint16_t holdoffTicks     = 4;                                          // holdoff after TIM1 power pulse
static const uint16_t tempMeasureTicks = 1;                                          // measurement period
uint16_t              totalPWM         = powerPWM + tempMeasureTicks + holdoffTicks; // TIM4 init period, the full PWM cycle

uint16_t ADCReadings[ADC_SAMPLES]; // Used to store the adc readings for the handle cold junction temp

uint16_t getADCHandleTemp(uint8_t sample) {
  static history<uint16_t, ADC_FILTER_LEN> filter = {{0}, 0, 0};
  if (sample) {
    uint32_t sum = 0;
    for (uint8_t i = 2; i < ADC_SAMPLES; i += ADC_CHANNELS) {
      sum += ADCReadings[i];
    }
    filter.update(sum);
  }
  return filter.average();
}

uint16_t getHandleTemperature(uint8_t sample) {
  // Temperature slope is 0.004V per 1C
  // ADC reports 13107 (1.32V) at 25C
  // That gives roughly 39.7 counts per 1C
  // Temperature in C is calculated as (13107-ADC)/39.7+25
  // We want to return above value times 10 so simplified it's ~ 3551-ADC/4
  return 3551 - getADCHandleTemp(sample) / 4;
}

uint16_t getADCVin(uint8_t sample) {
  // Use regular channels as Vin source
  static history<uint16_t, ADC_FILTER_LEN> filter = {{0}, 0, 0};
  if (sample) {
    uint32_t sum = 0;
    for (uint8_t i = 3; i < ADC_SAMPLES; i += ADC_CHANNELS) {
      sum += ADCReadings[i];
    }
    filter.update(sum);
  }
  return filter.average();
}
uint16_t getADCCurrent(uint8_t sample) {
  static history<uint16_t, ADC_FILTER_LEN> filter = {{0}, 0, 0};
  if (sample) {
    uint32_t sum = 0;
    for (uint8_t i = 1; i < ADC_SAMPLES; i += ADC_CHANNELS) {
      sum += ADCReadings[i];
    }
    filter.update(sum);
  }
  return filter.average();
}

uint16_t getInputVoltageX10(uint16_t divisor, uint8_t sample) {
  // VIN ADC maximum is 32768 == 3.3V at input == 36.3V (1:11 divider)
  // Correct divider here seems to be 90.27
  // Let's multiply times 6 here so the calibration value can start at around 540
  uint32_t res = getADCVin(sample);
  res *= 6;
  res /= divisor;
  return res;
}

// Returns either average or instant value. When sample is set the samples from the injected ADC are copied to the filter and then the raw reading is returned
uint16_t getTipRawTemp(uint8_t sample) {
  static history<uint16_t, ADC_FILTER_LEN> filter = {{0}, 0, 0};
  if (sample) {
    uint16_t latestADC = 0;

    latestADC += ADC->JDAT1;
    latestADC += ADC->JDAT2;
    latestADC += ADC->JDAT3;
    latestADC += ADC->JDAT4;
    latestADC *= 2; // pretend we're doing x8 oversampling
    filter.update(latestADC);
  }
  return filter.average();
}

static void switchToFastPWM(void) {
  // 20Hz, no slow PWM available
  totalPWM     = powerPWM + tempMeasureTicks + holdoffTicks;
  TIM4->AR     = totalPWM - 1;
  TIM4->CCDAT1 = powerPWM + holdoffTicks - 1;
  TIM4->CCDAT4 = powerPWM - 1;
  TIM4->PSC    = 3999; // 8kHz -> 125uS per tick
}

void setTipPWM(const uint16_t pulse, const bool shouldUseFastModePWM) {
  PWMSafetyTimer = 20; // This is decremented in the handler for PWM so that the tip pwm is
                       // disabled if the PID task is not scheduled often enough.

  uint16_t scaledPWM = (uint16_t)pulse * TIM1->AR / TIM4->CCDAT4; // We need to scale pulse from powerPWM to TIM1 period (394 -> 127)
  pendingPWM         = scaledPWM;
}

// The HS-02 has three physical buttons; IronOS's two logical buttons map to UP/DOWN,
// and the dedicated OK button is exposed via getButtonOK() so each UI screen can give
// it a context-appropriate action (see the BUTTON_OK_SHORT handlers in the UI logic).
uint8_t getButtonA() { return GPIO_ReadInputDataBit(BUTTON_Port, BUTTON_DOWN_Pin) == Bit_RESET ? 1 : 0; }
uint8_t getButtonB() { return GPIO_ReadInputDataBit(BUTTON_Port, BUTTON_UP_Pin) == Bit_RESET ? 1 : 0; }
uint8_t getButtonOK() { return GPIO_ReadInputDataBit(BUTTON_Port, BUTTON_OK_Pin) == Bit_RESET ? 1 : 0; }

void BSPInit(void) { switchToFastPWM(); }

void resetWatchdog() { IWDG_ReloadKey(); }

void reboot() { NVIC_SystemReset(); }

void delay_ms(uint16_t count) {
  for (volatile uint32_t i = 0; i < count * 6000; i++) {
  }
}

uint8_t       lastTipResistance        = 0; // default to unknown
const uint8_t numTipResistanceReadings = 3;
uint32_t      tipResistanceReadings[3] = {0, 0, 0};
uint8_t       tipResistanceReadingSlot = 0;
bool          isTipDisconnected() {
  uint16_t tipDisconnectedThres = TipThermoModel::getTipMaxInC() - 50;
  uint32_t tipTemp              = TipThermoModel::getTipInC();
  return tipTemp > tipDisconnectedThres;
}

void setStatusLED(const enum StatusLED state) {
  static bool       led;
  static TickType_t last;
  const TickType_t  now   = xTaskGetTickCount();
  const TickType_t  delta = now - last;

  switch (state) {
  case LED_HOT:
  case LED_HEATING:
    led = true;
    break;

  case LED_COOLING_STILL_HOT:
    if (delta >= 500) {
      led  = !led;
      last = now;
    }
    break;

  case LED_STANDBY:
  case LED_OFF:
  default:
    led = false;
    break;
  }

  if (led) {
    GPIO_SetBits(LED_Port, LED2_Pin);
  } else {
    GPIO_ResetBits(LED_Port, LED2_Pin);
  }
}

void setBuzzer(bool on) {
  // 63 = 50% duty cycle -> too lound and too much current
  // 20 = 16% duty cycle -> max reasonable
  static_assert(BUZZER_VOLUME < 20);
  TIM_SetCmp1(TIM2, on ? BUZZER_VOLUME : 0);
}

void setBuzzerVolume(uint8_t volume) {
  TIM_SetCmp1(TIM2, volume < 20 ? volume : 20);
}

uint8_t preStartChecks() { return CH224KVoltageSettled() ? 1 : 0; }

uint8_t preStartChecksDone() { return 1; }

uint8_t getTipResistanceX10() {
  const uint8_t userSelected = getUserSelectedTipResistance();
  return userSelected ? userSelected : TIP_RESISTANCE;
}

// N32L403 ID is either 96 or 128 bit long. Let's use first 64 bits of the 96bit one for now.
uint64_t getDeviceID() {
  union {
    uint8_t  bytes[UID_LENGTH];
    uint64_t deviceID;
  } uid;
  GetUID(uid.bytes);
  return (uid.deviceID);
}

bool isTipShorted() { return false; }
uint16_t getTipThermalMass() { return TIP_THERMAL_MASS; }
uint16_t getTipInertia() { return TIP_THERMAL_INERTIA; }

void showBootLogo(void) {
  const uint16_t durationSetting = getSettingValue(SettingsOptions::LOGOTime);
  if (durationSetting == logoMode_t::SKIP) {
    return;
  }

  LCD::drawIndexedImage(FNIRSI_LOGO_X, FNIRSI_LOGO_Y, FNIRSI_LOGO_W, FNIRSI_LOGO_H, fnirsiBootLogoFrame0, fnirsiBootLogoPalette);
  vTaskDelay(TICKS_SECOND / 60);
  LCD::setBrightness(getSettingValue(SettingsOptions::DisplayBrightness));

  const bool       infinite = (durationSetting == logoMode_t::INFINITY);
  const TickType_t duration = (durationSetting == logoMode_t::ONETIME) ? TICKS_SECOND : (TICKS_SECOND * durationSetting);
  const TickType_t start    = xTaskGetTickCount();

  // Eat any button still held from boot
  while (getButtonState() != BUTTON_NONE) {
    vTaskDelay(TICKS_10MS);
  }

  uint8_t    frame         = 0;
  TickType_t lastFrameTick = xTaskGetTickCount();
  for (;;) {
    const TickType_t now = xTaskGetTickCount();
    if (!infinite && (now - start) >= duration) {
      break;
    }
    if (getButtonState() != BUTTON_NONE) {
      break;
    }
    if ((now - lastFrameTick) >= (TICKS_10MS * (FNIRSI_LOGO_FRAME_MS / 10))) {
      lastFrameTick = now;
      frame         = (uint8_t)((frame + 1) % FNIRSI_LOGO_FRAME_COUNT);
      LCD::drawIndexedImage(FNIRSI_LOGO_ANIM_X, FNIRSI_LOGO_ANIM_Y, FNIRSI_LOGO_ANIM_W, FNIRSI_LOGO_ANIM_H, fnirsiBootLogoAnimFrames[frame],
                            fnirsiBootLogoPalette);
    }
    vTaskDelay(TICKS_10MS);
  }

  if (durationSetting == logoMode_t::ONETIME) {
    setSettingValue(SettingsOptions::LOGOTime, logoMode_t::SKIP);
    saveSettings();
  }
}
bool getFUS302IRQLow() { return false; }

void unstick_I2C() { /* What is brown and sticky? A stick. */ }
