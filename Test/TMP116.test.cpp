/**
 ******************************************************************************
 * @file			: TMP116.test.cpp
 * @brief			: TMP116 Tests
 * @author			: Lawrence Stanton
 ******************************************************************************
 */

#include "TMP116.hpp"

#include "au/units/fahrenheit.hh"
#include "au/units/kelvins.hh"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <cmath>

#include "../Src/TMP116.cpp"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Eq;
using ::testing::Return;
using ::testing::ReturnArg;

using std::nullopt;

using DeviceAddress = TMP116::I2C::DeviceAddress;
using MemoryAddress = TMP116::I2C::MemoryAddress;
using Register		= TMP116::I2C::Register;

// Tests of Static Functions

TEST(TMP116_TestStatic, decodeTemperatureRegisterReturnsCorrectValues) {
	EXPECT_FLOAT_EQ(decodeTemperatureRegister(static_cast<Register>(0x0000u)).in(au::celsius_pt), 0.0f);
	EXPECT_FLOAT_EQ(decodeTemperatureRegister(static_cast<Register>(0x0001u)).in(au::celsius_pt), 0.0078125f);
	EXPECT_FLOAT_EQ(decodeTemperatureRegister(static_cast<Register>(0x8000u)).in(au::celsius_pt), -256.0f);
	EXPECT_FLOAT_EQ(decodeTemperatureRegister(static_cast<Register>(0x8001u)).in(au::celsius_pt), -255.9921875f);
	EXPECT_FLOAT_EQ(decodeTemperatureRegister(static_cast<Register>(0xFFFFu)).in(au::celsius_pt), -0.0078125f);
	EXPECT_FLOAT_EQ(decodeTemperatureRegister(static_cast<Register>(0x7FFFu)).in(au::celsius_pt), 255.9921875f);
	EXPECT_FLOAT_EQ(decodeTemperatureRegister(static_cast<Register>(0x7FFEu)).in(au::celsius_pt), 255.984375f);
}

TEST(TMP116_TestStatic, encodeTemperatureRegisterReturnsCorrectValues) {
	EXPECT_EQ(encodeTemperatureRegister(au::celsius_pt(0.0f)), static_cast<Register>(0x0000u));
	EXPECT_EQ(encodeTemperatureRegister(au::celsius_pt(0.0078125f)), static_cast<Register>(0x0001u));
	EXPECT_EQ(encodeTemperatureRegister(au::celsius_pt(-0.0078125f)), static_cast<Register>(0xFFFFu));
	EXPECT_EQ(encodeTemperatureRegister(au::celsius_pt(-0.015625f)), static_cast<Register>(0xFFFEu));
	EXPECT_EQ(encodeTemperatureRegister(au::celsius_pt(-256.0f)), static_cast<Register>(0x8000u));
	EXPECT_EQ(encodeTemperatureRegister(au::celsius_pt(-255.9921875f)), static_cast<Register>(0x8001u));
	EXPECT_EQ(encodeTemperatureRegister(au::celsius_pt(255.9921875f)), static_cast<Register>(0x7FFFu));
	EXPECT_EQ(encodeTemperatureRegister(au::celsius_pt(255.984375f)), static_cast<Register>(0x7FFEu));
}

TEST(TMP116_TestStatic, encodeTemperatureRegisterRoundsToNearestLsb) {
	EXPECT_EQ(encodeTemperatureRegister(au::celsius_pt(0.0039f)), static_cast<Register>(0x0000u));
	EXPECT_EQ(encodeTemperatureRegister(au::celsius_pt(0.0040f)), static_cast<Register>(0x0001u));
	EXPECT_EQ(encodeTemperatureRegister(au::celsius_pt(0.0077f)), static_cast<Register>(0x0001u));
	EXPECT_EQ(encodeTemperatureRegister(au::celsius_pt(-0.0040f)), static_cast<Register>(0xFFFFu));
}

TEST(TMP116_TestStatic, encodeTemperatureRegisterClampsToRegisterRange) {
	EXPECT_EQ(encodeTemperatureRegister(au::celsius_pt(256.0f)), static_cast<Register>(0x7FFFu));
	EXPECT_EQ(encodeTemperatureRegister(au::celsius_pt(1000.0f)), static_cast<Register>(0x7FFFu));
	EXPECT_EQ(encodeTemperatureRegister(au::celsius_pt(-1000.0f)), static_cast<Register>(0x8000u));
	EXPECT_EQ(encodeTemperatureRegister(au::celsius_pt(INFINITY)), static_cast<Register>(0x7FFFu));
	EXPECT_EQ(encodeTemperatureRegister(au::celsius_pt(-INFINITY)), static_cast<Register>(0x8000u));
}

TEST(TMP116_TestStatic, encodeTemperatureRegisterEncodesNanAsZero) {
	EXPECT_EQ(encodeTemperatureRegister(au::celsius_pt(NAN)), static_cast<Register>(0x0000u));
}

TEST(TMP116_TestStatic, encodeTemperatureRegisterIsIndependentOfInputUnit) {
	const Register oneHundredCelsius = 0x3200u;
	EXPECT_EQ(encodeTemperatureRegister(au::celsius_pt(100.0f)), oneHundredCelsius);
	EXPECT_EQ(encodeTemperatureRegister(au::fahrenheit_pt(212.0f)), oneHundredCelsius);
	EXPECT_EQ(encodeTemperatureRegister(au::kelvins_pt(373.15f)), oneHundredCelsius);
	EXPECT_EQ(encodeTemperatureRegister(au::fahrenheit_pt(-40.0f)), encodeTemperatureRegister(au::celsius_pt(-40.0f)));
}

// Tests of Member Functions

class MockedI2C : public TMP116::I2C {
public:
	MOCK_METHOD(
		std::optional<Register>, //
		read,
		(DeviceAddress deviceAddress, MemoryAddress memoryAddress),
		(override)
	);
	MOCK_METHOD(
		std::optional<Register>,
		write,
		(DeviceAddress deviceAddress, MemoryAddress memoryAddress, Register registerValue),
		(override)
	);
};

class TMP116_Test : public ::testing::Test {
public:
	MockedI2C			  mockedI2C{};
	TMP116::DeviceAddress deviceAddress = TMP116::DeviceAddress::ADD0_GND;
	TMP116				  tmp116{mockedI2C, deviceAddress};

	inline void disableI2C(void) {
		EXPECT_CALL(mockedI2C, read).Times(AnyNumber()).WillRepeatedly(Return(nullopt));
		EXPECT_CALL(mockedI2C, write).Times(AnyNumber()).WillRepeatedly(Return(nullopt));
	}
};

TEST_F(TMP116_Test, getTemperatureNormallyReturnsValue) {
	const MemoryAddress temperatureAddress				   = 0x00u;
	const Register		temperatureRegisterTestRandomValue = 0x15D2u;
	const float			expectedCelsius					   = 43.640625f;
	const float			expectedFahrenheit				   = 110.553125f;
	EXPECT_CALL(mockedI2C, read(Eq(this->tmp116.getDeviceAddress()), Eq(temperatureAddress)))
		.WillOnce(Return(temperatureRegisterTestRandomValue));

	const auto temperature = this->tmp116.getTemperature();
	ASSERT_TRUE(temperature.has_value());
	EXPECT_FLOAT_EQ(temperature->in(au::celsius_pt), expectedCelsius);
	EXPECT_NEAR(temperature->in(au::fahrenheit_pt), expectedFahrenheit, 1e-4f);
}

TEST_F(TMP116_Test, getTemperatureReturnsNulloptWhenI2CReadFails) {
	this->disableI2C();
	EXPECT_FALSE(this->tmp116.getTemperature().has_value());
}

TEST_F(TMP116_Test, getDeviceIdNormallyReturnsValue) {
	const MemoryAddress deviceIdAddress		 = 0x0Fu;
	const Register		deviceIdDefaultValue = 0x0118u;
	EXPECT_CALL(mockedI2C, read(Eq(this->tmp116.getDeviceAddress()), Eq(deviceIdAddress)))
		.WillOnce(Return(deviceIdDefaultValue));

	const auto deviceId = this->tmp116.getDeviceId();
	EXPECT_EQ(deviceId.value(), deviceIdDefaultValue);
}

TEST_F(TMP116_Test, getDeviceIdReturnsNulloptWhenI2CReadFails) {
	this->disableI2C();
	EXPECT_EQ(this->tmp116.getDeviceId(), nullopt);
}

TEST_F(TMP116_Test, getConfigRegisterNormallyReturnsValue) {
	const MemoryAddress configAddress			  = 0x01u;
	const Register		configDefaultValue		  = 0x0220u;
	const Register		configExpectedReturnValue = 0x0220u;
	EXPECT_CALL(mockedI2C, read(Eq(this->tmp116.getDeviceAddress()), Eq(configAddress)))
		.WillOnce(Return(configDefaultValue));

	const auto config = this->tmp116.getConfigRegister();
	EXPECT_EQ(config.value(), configExpectedReturnValue);
}

TEST_F(TMP116_Test, getConfigRegisterReturnsNulloptWhenI2CReadFails) {
	this->disableI2C();
	EXPECT_EQ(this->tmp116.getConfigRegister(), nullopt);
}

TEST_F(TMP116_Test, getConfigNormallyReturnsValue) {
	const MemoryAddress configAddress	   = 0x01u;
	const Register		configDefaultValue = 0x0220u;

	EXPECT_CALL(mockedI2C, read(Eq(this->tmp116.getDeviceAddress()), Eq(configAddress)))
		.WillOnce(Return(configDefaultValue));

	const auto config = this->tmp116.getConfig();
	EXPECT_TRUE(config.has_value());
	EXPECT_EQ(config, Config{configDefaultValue});
}

TEST_F(TMP116_Test, getConfigReturnsNulloptWhenI2CReadFails) {
	this->disableI2C();
	EXPECT_EQ(this->tmp116.getConfig(), nullopt);
}

TEST_F(TMP116_Test, dataReadyNormallyReturnsValue) {
	EXPECT_CALL(mockedI2C, read).WillOnce(Return(0x0220u)).WillOnce(Return(0x2220u));
	EXPECT_FALSE(this->tmp116.dataReady().value());
	EXPECT_TRUE(this->tmp116.dataReady().value());
}

TEST_F(TMP116_Test, dataReadyReturnsFalseWhenI2CReadFails) {
	this->disableI2C();
	EXPECT_EQ(this->tmp116.dataReady(), nullopt);
}

TEST_F(TMP116_Test, setConfigNormallyReturnsRegisterValue) {
	const MemoryAddress configAddress				= 0x01u;
	const Register		configDefaultValue			= 0x0220u;
	const Register		configExpectedWrittenValue	= 0x0220u;
	const Config		configExpectedWrittenConfig = Config{configExpectedWrittenValue};

	EXPECT_CALL(mockedI2C, write(Eq(this->deviceAddress), Eq(configAddress), Eq(configExpectedWrittenValue)))
		.WillOnce(ReturnArg<2>());

	const auto configResult = this->tmp116.setConfig(configExpectedWrittenConfig);
	EXPECT_EQ(configResult.value(), configExpectedWrittenValue);
}

TEST_F(TMP116_Test, setConfigReturnsNulloptWhenI2CWriteFails) {
	this->disableI2C();
	EXPECT_EQ(this->tmp116.setConfig(Config{}), nullopt);
}

TEST_F(TMP116_Test, setConfigNormallyDirectlyWritesWhenGivenCompleteNewConfiguration) {
	const MemoryAddress configAddress			   = 0x01u;
	const Register		configDefaultValue		   = 0x0220u;
	const Register		configExpectedWrittenValue = 0x0FFCu;

	EXPECT_CALL(mockedI2C, read).Times(0);
	EXPECT_CALL(mockedI2C, write(Eq(this->deviceAddress), Eq(configAddress), _)).WillOnce(ReturnArg<2>());

	const auto configResult = this->tmp116.setConfig(
		Config::TemperatureConversionMode::ONESHOT,
		Config::ConversionCycleTime::CONV_16000MS,
		Config::Averages::AVG_64,
		Config::ThermalAlertModeSelect::THERM,
		Config::AlertPolarity::ACTIVE_HIGH,
		Config::DataReadyAlertPinSelect::DATA_READY
	);

	EXPECT_EQ(configResult.value(), configExpectedWrittenValue);
}

TEST_F(TMP116_Test, setConfigReturnsNulloptIfGivenNoParameters) {
	EXPECT_EQ(this->tmp116.setConfig(), nullopt);
}

TEST_F(TMP116_Test, setConfigSetsOnlyGivenParametersAndOtherwiseMaintainsState) {
	const MemoryAddress configAddress			   = 0x01u;
	const Register		configDefaultValue		   = 0x0220u;
	const Register		configExpectedWrittenValue = 0x023Cu;

	EXPECT_CALL(mockedI2C, read).WillOnce(Return(configDefaultValue));
	EXPECT_CALL(mockedI2C, write(Eq(this->deviceAddress), Eq(configAddress), _)).WillOnce(ReturnArg<2>());

	const auto configResult = this->tmp116.setConfig(
		{},
		{},
		{},
		Config::ThermalAlertModeSelect::THERM,
		Config::AlertPolarity::ACTIVE_HIGH,
		Config::DataReadyAlertPinSelect::DATA_READY
	);

	EXPECT_EQ(configResult.value(), configExpectedWrittenValue);
}

TEST_F(TMP116_Test, setConfigWillNotWriteIfConfigIsUnchangedAndWillReturnCurrentConfig) {
	const MemoryAddress configAddress			   = 0x01u;
	const Register		configDefaultValue		   = 0x0220u;
	const Register		configExpectedWrittenValue = 0x0220u;

	EXPECT_CALL(mockedI2C, read(Eq(this->deviceAddress), Eq(configAddress))).WillOnce(Return(configDefaultValue));
	EXPECT_CALL(mockedI2C, write).Times(0);

	const auto configResult = this->tmp116.setConfig(
		{}, //
		Config::ConversionCycleTime::CONV_1000MS,
		Config::Averages::AVG_8,
		{},
		{},
		{}
	);

	EXPECT_EQ(configResult.value(), configExpectedWrittenValue);
}

TEST_F(TMP116_Test, setConfigReturnsNulloptWhenI2CFails) {
	this->disableI2C();
	EXPECT_EQ(this->tmp116.setConfig(Config{}), nullopt);
}

TEST_F(TMP116_Test, setHighLimitNormallyReturnsRegisterValue) {
	const MemoryAddress highLimitAddress			  = 0x02u;
	const Register		highLimitExpectedWrittenValue = 0xFB00u;

	EXPECT_CALL(mockedI2C, write(Eq(this->deviceAddress), Eq(highLimitAddress), Eq(highLimitExpectedWrittenValue)))
		.WillOnce(ReturnArg<2>());

	const auto highLimitResult = this->tmp116.setHighLimit(au::celsius_pt(-10.0f));
	EXPECT_EQ(highLimitResult.value(), highLimitExpectedWrittenValue);
}

TEST_F(TMP116_Test, setHighLimitWritesSameRegisterForEquivalentTemperaturesInAnyUnit) {
	const MemoryAddress highLimitAddress			  = 0x02u;
	const Register		highLimitExpectedWrittenValue = 0x3200u; // 100 degrees Celsius

	EXPECT_CALL(mockedI2C, write(Eq(this->deviceAddress), Eq(highLimitAddress), Eq(highLimitExpectedWrittenValue)))
		.Times(3)
		.WillRepeatedly(ReturnArg<2>());

	EXPECT_EQ(this->tmp116.setHighLimit(au::celsius_pt(100.0f)).value(), highLimitExpectedWrittenValue);
	EXPECT_EQ(this->tmp116.setHighLimit(au::fahrenheit_pt(212.0f)).value(), highLimitExpectedWrittenValue);
	EXPECT_EQ(this->tmp116.setHighLimit(au::kelvins_pt(373.15f)).value(), highLimitExpectedWrittenValue);
}

TEST_F(TMP116_Test, setHighLimitReturnsNulloptWhenI2CWriteFails) {
	this->disableI2C();
	EXPECT_EQ(this->tmp116.setHighLimit(au::celsius_pt(0.0f)), nullopt);
}

TEST_F(TMP116_Test, setLowLimitNormallyReturnsRegisterValue) {
	const MemoryAddress lowLimitAddress				 = 0x03u;
	const Register		lowLimitExpectedWrittenValue = 0xFB00u;

	EXPECT_CALL(mockedI2C, write(Eq(this->deviceAddress), Eq(lowLimitAddress), Eq(lowLimitExpectedWrittenValue)))
		.WillOnce(ReturnArg<2>());

	const auto lowLimitResult = this->tmp116.setLowLimit(au::celsius_pt(-10.0f));
	EXPECT_EQ(lowLimitResult.value(), lowLimitExpectedWrittenValue);
}

TEST_F(TMP116_Test, setLowLimitWritesSameRegisterForEquivalentTemperaturesInAnyUnit) {
	const MemoryAddress lowLimitAddress				 = 0x03u;
	const Register		lowLimitExpectedWrittenValue = 0xEC00u; // -40 degrees Celsius

	EXPECT_CALL(mockedI2C, write(Eq(this->deviceAddress), Eq(lowLimitAddress), Eq(lowLimitExpectedWrittenValue)))
		.Times(3)
		.WillRepeatedly(ReturnArg<2>());

	EXPECT_EQ(this->tmp116.setLowLimit(au::celsius_pt(-40.0f)).value(), lowLimitExpectedWrittenValue);
	EXPECT_EQ(this->tmp116.setLowLimit(au::fahrenheit_pt(-40.0f)).value(), lowLimitExpectedWrittenValue);
	EXPECT_EQ(this->tmp116.setLowLimit(au::kelvins_pt(233.15f)).value(), lowLimitExpectedWrittenValue);
}

TEST_F(TMP116_Test, setLowLimitReturnsNulloptWhenI2CWriteFails) {
	this->disableI2C();
	EXPECT_EQ(this->tmp116.setLowLimit(au::celsius_pt(0.0f)), nullopt);
}

TEST_F(TMP116_Test, getHighLimitNormallyReturnsValue) {
	const MemoryAddress highLimitAddress	   = 0x02u;
	const Register		highLimitRegisterValue = 0x3200u; // 100 degrees Celsius

	EXPECT_CALL(mockedI2C, read(Eq(this->deviceAddress), Eq(highLimitAddress))).WillOnce(Return(highLimitRegisterValue));

	const auto highLimit = this->tmp116.getHighLimit();
	ASSERT_TRUE(highLimit.has_value());
	EXPECT_FLOAT_EQ(highLimit->in(au::celsius_pt), 100.0f);
	EXPECT_NEAR(highLimit->in(au::fahrenheit_pt), 212.0f, 1e-4f);
}

TEST_F(TMP116_Test, getHighLimitReturnsNulloptWhenI2CReadFails) {
	this->disableI2C();
	EXPECT_FALSE(this->tmp116.getHighLimit().has_value());
}

TEST_F(TMP116_Test, getLowLimitNormallyReturnsValue) {
	const MemoryAddress lowLimitAddress		  = 0x03u;
	const Register		lowLimitRegisterValue = 0xFB00u; // -10 degrees Celsius

	EXPECT_CALL(mockedI2C, read(Eq(this->deviceAddress), Eq(lowLimitAddress))).WillOnce(Return(lowLimitRegisterValue));

	const auto lowLimit = this->tmp116.getLowLimit();
	ASSERT_TRUE(lowLimit.has_value());
	EXPECT_FLOAT_EQ(lowLimit->in(au::celsius_pt), -10.0f);
	EXPECT_NEAR(lowLimit->in(au::fahrenheit_pt), 14.0f, 1e-4f);
}

TEST_F(TMP116_Test, getLowLimitReturnsNulloptWhenI2CReadFails) {
	this->disableI2C();
	EXPECT_FALSE(this->tmp116.getLowLimit().has_value());
}
