/**
 ******************************************************************************
 * @file			: TMP116_Convert.hpp
 * @brief			: TMP116 temperature register encoding
 ******************************************************************************
 */

#pragma once

#include "TMP116.hpp"

#include <cstdint>

#include <mp-units/systems/si.h>

/**
 * @brief Implementation detail of the TMP116 driver: the temperature register encoding.
 *
 * @note This lives in a header, rather than in TMP116.cpp, so that the driver and its unit tests
 * share a single definition. The functions are constexpr and therefore implicitly inline, so
 * including this in several translation units defines each of them exactly once.
 */
namespace TMP116_Detail {

/** @brief The temperature change represented by one LSB of a TMP116 temperature register. */
inline constexpr float lsbResolutionDegreesCelsius = 0.0078125f;

/** @brief The range of the signed 16 bit two's complement value the temperature registers hold. */
inline constexpr float minimumRegisterLsbs = -32768.0f;
inline constexpr float maximumRegisterLsbs = 32767.0f;

/**
 * @brief Convert a TMP116 temperature register value to a temperature.
 *
 * @param registerValue The TMP116 register value.
 * @return TMP116::Temperature The equivalent temperature.
 */
constexpr TMP116::Temperature convertTemperatureRegister(TMP116::Register registerValue) {
	const float degreesCelsius =
		static_cast<float>(static_cast<int16_t>(registerValue)) * lsbResolutionDegreesCelsius;
	return mp_units::point<mp_units::si::degree_Celsius>(degreesCelsius);
}

/**
 * @brief Convert a temperature to a TMP116 temperature register value.
 *
 * @param temperature The temperature, in any unit; the conversion to degrees Celsius is implicit.
 * @return TMP116::Register The TMP116 register equivalent value.
 * @note The result saturates at the extremes the register can encode. Converting an out of range
 * or NaN float to int16_t is undefined, so the value is brought into range beforehand.
 */
constexpr TMP116::Register convertTemperatureRegister(TMP116::Temperature temperature) {
	const float lsbs =
		temperature.quantity_from_zero().numerical_value_in(mp_units::si::degree_Celsius) /
		lsbResolutionDegreesCelsius;

	// Round half away from zero, rather than truncating towards it, so that the encoded value is
	// always the closest one the register can represent.
	const float rounded = (lsbs < 0.0f) ? (lsbs - 0.5f) : (lsbs + 0.5f);

	// Every comparison against a NaN is false, so a NaN temperature takes none of the first three
	// branches and encodes as 0 degC.
	float saturated = 0.0f;
	if ((rounded >= minimumRegisterLsbs) && (rounded <= maximumRegisterLsbs)) saturated = rounded;
	else if (rounded > maximumRegisterLsbs) saturated = maximumRegisterLsbs;
	else if (rounded < minimumRegisterLsbs) saturated = minimumRegisterLsbs;

	return static_cast<TMP116::Register>(static_cast<int16_t>(saturated));
}

} // namespace TMP116_Detail
