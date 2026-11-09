/*
 * ThermoModel.cpp
 *
 *  Created on: 1 May 2021
 *      Author: Ralim, MrTick
 */
#include "BSP.h"
#include "Settings.h"
#include "Setup.h"
#include "TipThermoModel.h"
#include "Types.h"
#include "Utils.hpp"
#include "configuration.h"

extern uint16_t tipSenseResistancex10Ohms;

/*
 * HS-02 factory temperature calibration (from stock firmware reverse engineering).
 *
 * The stock firmware stores three per-unit calibration words in its settings page
 * at flash 0x0801F800 (word offsets 0x16/0x17/0x18 = byte offsets 0x58/0x5C/0x60).
 * Each word minus a fixed bias yields the raw 12-bit ADC count measured at a
 * reference tip temperature during factory calibration:
 *
 *   count(140C) = word[0x16] - bias140
 *   count(240C) = word[0x17] - bias240
 *   count(340C) = word[0x18] - bias340
 *
 * Each bias is 0x8000 minus the model's NOMINAL count, so the uncalibrated 0x8000
 * placeholder decodes to the nominal curve. The biases differ per model (stock V1.8
 * temperature function, 0x08007408 on A / 0x080073ec on B):
 *
 *            bias140/240/340          nominal counts at 140/240/340C
 *   HS-02A   0x7F21/0x7E77/0x7DC4     223/393/572
 *   HS-02B   0x7FBC/0x7F8B/0x7F42      68/117/190
 *
 * The B's thermocouple signal at the ADC is ~3.2x smaller than the A's for the same
 * tip temperature, so the A's curve read a red-hot B tip as ~200C. Everything else
 * in that function is identical, and the ADC read and Vin divider match too.
 *
 * Stock feeds the RAW count into the curve: one ADC conversion, then straight into the
 * compares below, with no zero offset removed first. IronOS removes its CJC offset
 * setting before it calls us, so stockRawTipuV() puts that back.
 *
 * Stock converts ADC counts to degC with a piecewise-linear curve through these
 * points (the thermocouple's uV/C slope rises with temperature, so a single
 * constant slope reads increasingly high in the soldering range). The curve starts at
 * 0C for a count of 0, and stock then adds a constant 25C to get the tip temperature
 * it displays and regulates, so the three points are tip temperatures of 165/265/365C.
 * The A continues with a fourth, fixed point (770 counts = 440C on the curve); the B
 * extrapolates its last segment. All of this is checked against the stock images
 * running in tools/hs02-stock-emu.
 *
 * Every unit uses this curve, including units with erased calibration pages or the
 * stock 0x8000 placeholders. The nominal placeholders decode to counts 223/393/572
 * on HS-02A and 68/117/190 on HS-02B. Stock always adds 25C after the curve.
 * IronOS keeps its own settings at 0x0801E000-0x0801F000, so the stock page survives
 * flashing IronOS and can provide per-unit calibration points.
 */
namespace {
constexpr uint32_t stockSettingsPageAddr = 0x0801F800;
#ifdef MODEL_HS02B
constexpr uint32_t stockCalBias140 = 0x7FBC;
constexpr uint32_t stockCalBias240 = 0x7F8B;
constexpr uint32_t stockCalBias340 = 0x7F42;
constexpr int32_t  minSaneCount140 = 20;
constexpr uint32_t stockCount440   = 0; // none: the B extrapolates above the 340C point
#else
constexpr uint32_t stockCalBias140 = 0x7F21;
constexpr uint32_t stockCalBias240 = 0x7E77;
constexpr uint32_t stockCalBias340 = 0x7DC4;
constexpr int32_t  minSaneCount140 = 50;
constexpr uint32_t stockCount440   = 770;
#endif
constexpr uint32_t stockCurveOffsetCx10 = 250; // stock tip temperature = curve + 25C

bool     factoryCalChecked     = false;
bool     factoryCalValid       = false;
bool     factoryCalCustomized  = false;
uint32_t adcCount140           = 0; // Raw 12-bit ADC counts at the three factory reference points
uint32_t adcCount240           = 0;
uint32_t adcCount340           = 0;
uint32_t curveOffsetCx10       = 0;

void useNominalCal() {
  adcCount140     = 0x8000 - stockCalBias140;
  adcCount240     = 0x8000 - stockCalBias240;
  adcCount340     = 0x8000 - stockCalBias340;
  curveOffsetCx10 = stockCurveOffsetCx10;
  factoryCalValid = true;
}

// The stock curve wants the raw reading, but tipuVDelta already had the CJC offset
// setting removed. That setting defaults to 900uV = 64 stock counts, which is nearly the
// whole 68-count signal of an HS-02B at 140C (and 29% of an A's), so a stock-calibrated
// unit read far too low and by a different amount on every unit that had run the CJC
// calibration. A reading that was clamped to zero lies somewhere below the offset, so
// take that one from the ADC (same integer steps as convertTipRawADCTouV).
uint32_t stockRawTipuV(uint32_t tipuVDelta) {
  const uint32_t offsetuV = getSettingValue(SettingsOptions::CalibrationOffset);
  if (tipuVDelta > 0) {
    return tipuVDelta + offsetuV;
  }
  const uint32_t rawuV = (((uint32_t)getTipRawTemp(0) * ADC_VDD_MV * 10) / ADC_MAX_READING) * 100 / OP_AMP_GAIN_STAGE;
  return (rawuV < offsetuV) ? rawuV : offsetuV;
}

void loadFactoryCal() {
  factoryCalChecked = true;
  useNominalCal();
  const uint32_t *page = (const uint32_t *)stockSettingsPageAddr;
  if (page[0] == 0xFFFFFFFF) {
    return;
  }

  const uint32_t word140 = page[0x16] & 0xFFFF;
  const uint32_t word240 = page[0x17] & 0xFFFF;
  const uint32_t word340 = page[0x18] & 0xFFFF;
  if (word140 == 0x8000 && word240 == 0x8000 && word340 == 0x8000) {
    return;
  }

  const int32_t a140 = (int32_t)word140 - (int32_t)stockCalBias140;
  const int32_t a240 = (int32_t)word240 - (int32_t)stockCalBias240;
  const int32_t a340 = (int32_t)word340 - (int32_t)stockCalBias340;
  if (a140 < minSaneCount140 || a240 <= a140 || a340 <= a240 || a340 > 1500) {
    return;
  }

  adcCount140          = (uint32_t)a140;
  adcCount240          = (uint32_t)a240;
  adcCount340          = (uint32_t)a340;
  factoryCalCustomized = true;
}

} // namespace

// Outputs the active calibration counts. The nominal stock curve is valid even when
// the stock page is erased, contains placeholders, or has unusable custom values.
bool hs02GetFactoryTipCal(uint32_t &a140, uint32_t &a240, uint32_t &a340) {
  if (!factoryCalChecked) {
    loadFactoryCal();
  }
  a140 = adcCount140;
  a240 = adcCount240;
  a340 = adcCount340;
  (void)factoryCalCustomized;
  return factoryCalValid;
}

TemperatureType_t TipThermoModel::convertuVToDegCx10(uint32_t tipuVDelta) {
  if (!factoryCalChecked) {
    loadFactoryCal();
  }

  // Convert IronOS uV back to stock 12-bit ADC counts (x10 for precision) by
  // inverting the exact forward conversion used in convertTipRawADCTouV:
  //   uV = raw * (ADC_VDD_MV * 1000 / ADC_MAX_READING) / OP_AMP_GAIN_STAGE
  // with stockCount = raw / 8 (the x8 oversampling).
  const uint32_t countX10 = (stockRawTipuV(tipuVDelta) * ((OP_AMP_GAIN_STAGE * 4096) / 33)) / 10000;

  uint32_t tempX10;
  if (countX10 <= adcCount140 * 10) {
    tempX10 = (countX10 * 140) / adcCount140;
  } else if (countX10 <= adcCount240 * 10) {
    tempX10 = 1400 + ((countX10 - adcCount140 * 10) * 100) / (adcCount240 - adcCount140);
  } else if (stockCount440 > adcCount340 && countX10 >= adcCount340 * 10) {
    tempX10 = 3400 + ((countX10 - adcCount340 * 10) * 100) / (stockCount440 - adcCount340);
  } else {
    tempX10 = 2400 + ((countX10 - adcCount240 * 10) * 100) / (adcCount340 - adcCount240);
  }
  tempX10 += curveOffsetCx10;
  const uint32_t handleTempCx10 = getHandleTemperature(0);
  return (tempX10 > handleTempCx10) ? (tempX10 - handleTempCx10) : 0;
}
