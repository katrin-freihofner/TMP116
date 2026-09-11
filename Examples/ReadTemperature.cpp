/**
 ******************************************************************************
 * @file			: ReadTemperature.cpp
 * @brief			: Example: poll the TMP116 and read temperatures with getTemperature()
 * @author			: Katrin Freihofner
 ******************************************************************************
 */

#include "TMP116.hpp"

#include <cstdio>

#include "au/units/fahrenheit.hh"

namespace {
/**
 * @brief A simulated TMP116 on an I2C bus, so this example runs on a host machine.
 *
 * @details On real hardware this class would forward read() and write() to the platform HAL instead, e.g.
 * HAL_I2C_Mem_Read() / HAL_I2C_Mem_Write() on STM32, returning std::nullopt when the HAL reports an error.
 * The simulated sensor warms by 0.5 degrees Celsius per conversion, and a conversion completes on every second
 * poll of the config register.
 */
class SimulatedI2C final : public TMP116::I2C {
public:
	bool busFault = false; // Set to make every transaction fail, as a disconnected sensor would.

	std::optional<Register> read(DeviceAddress deviceAddress, MemoryAddress memoryAddress) override {
		if (this->busFault || deviceAddress != DeviceAddress::ADD0_GND) return std::nullopt;

		switch (memoryAddress) {
		case 0x00u: // Temperature Register. Reading it clears the Data Ready Flag.
			this->dataReadyFlag = false;
			return this->temperatureRegister;
		case 0x01u: { // Config Register. Reading it clears the Data Ready Flag.
			this->advanceConversion();
			const Register value = this->configRegister | (this->dataReadyFlag ? 0x2000u : 0x0000u);
			this->dataReadyFlag	 = false;
			return value;
		}
		case 0x0Fu: // Device ID Register
			return 0x1116u;
		default:
			return 0x0000u;
		}
	}

	std::optional<Register> write(DeviceAddress deviceAddress, MemoryAddress memoryAddress, Register data) override {
		if (this->busFault || deviceAddress != DeviceAddress::ADD0_GND) return std::nullopt;

		if (memoryAddress == 0x01u) this->configRegister = data & 0x0FFFu; // Flags are read only.
		return data;
	}

private:
	Register configRegister		 = 0x0220u; // TMP116 power-on default: continuous, 1 s cycle, 8 averages.
	Register temperatureRegister = 0x0B80u; // 23.0 degrees Celsius at 0.0078125 degrees Celsius per LSB.
	bool	 dataReadyFlag		 = false;
	unsigned polls				 = 0;

	void advanceConversion() {
		if (++this->polls % 2 != 0) return;
		this->temperatureRegister = static_cast<Register>(this->temperatureRegister + 64u); // +0.5 degrees Celsius
		this->dataReadyFlag		  = true;
	}
};
} // namespace

int main() {
	SimulatedI2C i2c;
	TMP116		 sensor{i2c, TMP116::DeviceAddress::ADD0_GND};

	if (sensor.getDeviceId() != 0x1116u) {
		std::puts("TMP116 not found on the I2C bus.");
		return 1;
	}

	using Config = TMP116::Config;
	if (!sensor.setConfig(Config::TemperatureConversionMode::CONTINUOUS, Config::ConversionCycleTime::CONV_125MS)) {
		std::puts("Failed to configure the TMP116.");
		return 1;
	}

	const TMP116::Temperature warningThreshold = au::celsius_pt(24.5f);

	for (int samples = 0; samples < 5;) {
		const auto ready = sensor.dataReady();
		if (!ready) {
			std::puts("I2C error while polling the Data Ready Flag.");
			return 1;
		}
		if (!ready.value()) continue; // No new conversion yet. On real hardware, sleep or yield here.

		// getTemperature() returns std::nullopt if the I2C read fails, so always check before using the value.
		const auto temperature = sensor.getTemperature();
		if (!temperature) {
			std::puts("I2C error while reading the temperature.");
			return 1;
		}

		// The result is a typed Au quantity point: read it out in whichever unit you need.
		std::printf(
			"Temperature: %7.3f degC  %7.3f degF%s\n",
			temperature->in(au::celsius_pt),
			temperature->in(au::fahrenheit_pt),
			temperature.value() > warningThreshold ? "  (above warning threshold)" : ""
		);
		++samples;
	}

	// A failed transaction is reported as std::nullopt rather than a bogus temperature.
	i2c.busFault = true;
	if (!sensor.getTemperature()) std::puts("Bus fault: getTemperature() returned std::nullopt, as expected.");

	return 0;
}
