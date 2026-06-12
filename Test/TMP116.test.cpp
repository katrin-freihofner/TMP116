/**
 ******************************************************************************
 * @file			: TMP116.test.cpp
 * @brief			: TMP116 Tests
 * @author			: Lawrence Stanton
 ******************************************************************************
 */

#include "TMP116.hpp"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

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

TEST(TMP116_TestStatic, convertTemperatureRegisterReturnsCorrectValuesRegisterToFloat) {
	EXPECT_FLOAT_EQ(convertTemperatureRegister(static_cast<Register>(0x0000u)), 0.0f);
	EXPECT_FLOAT_EQ(convertTemperatureRegister(static_cast<Register>(0x0001u)), 0.0078125f);
	EXPECT_FLOAT_EQ(convertTemperatureRegister(static_cast<Register>(0x8000u)), -256.0f);
	EXPECT_FLOAT_EQ(convertTemperatureRegister(static_cast<Register>(0x8001u)), -255.9921875f);
	EXPECT_FLOAT_EQ(convertTemperatureRegister(static_cast<Register>(0xFFFFu)), -0.0078125f);
	EXPECT_FLOAT_EQ(convertTemperatureRegister(static_cast<Register>(0x7FFFu)), 255.9921875f);
	EXPECT_FLOAT_EQ(convertTemperatureRegister(static_cast<Register>(0x7FFEu)), 255.984375f);
}

TEST(TMP116_TestStatic, convertTemperatureRegisterReturnsCurrentValuesFloatToRegister) {
	EXPECT_EQ(convertTemperatureRegister(0.0f), static_cast<Register>(0x0000u));
	EXPECT_EQ(convertTemperatureRegister(0.0078125f), static_cast<Register>(0x0001u));
	EXPECT_EQ(convertTemperatureRegister(-0.0078125f), static_cast<Register>(0xFFFFu));
	EXPECT_EQ(convertTemperatureRegister(-0.015625f), static_cast<Register>(0xFFFEu));
	EXPECT_EQ(convertTemperatureRegister(-256.0f), static_cast<Register>(0x8000u));
	EXPECT_EQ(convertTemperatureRegister(-255.9921875f), static_cast<Register>(0x8001u));
	EXPECT_EQ(convertTemperatureRegister(255.9921875f), static_cast<Register>(0x7FFFu));
	EXPECT_EQ(convertTemperatureRegister(255.984375f), static_cast<Register>(0x7FFEu));
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
	const float			expectedTemperatureValue		   = 43.640625f;
	EXPECT_CALL(mockedI2C, read(Eq(this->tmp116.getDeviceAddress()), Eq(temperatureAddress)))
		.WillOnce(Return(temperatureRegisterTestRandomValue));

	const auto temperature = this->tmp116.getTemperature();
	EXPECT_FLOAT_EQ(temperature, expectedTemperatureValue);
}

TEST_F(TMP116_Test, getTemperatureReturnsAbsoluteZeroWhenI2CReadFails) {
	this->disableI2C();
	EXPECT_EQ(this->tmp116.getTemperature(), -256.0f);
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

TEST_F(TMP116_Test, setHighLimitReturnsTrueOnSuccess) {
	const MemoryAddress highLimitAddress			  = 0x02u;
	const Register		highLimitExpectedWrittenValue = 0xFB00u;

	EXPECT_CALL(mockedI2C, write(Eq(this->deviceAddress), Eq(highLimitAddress), Eq(highLimitExpectedWrittenValue)))
		.WillOnce(ReturnArg<2>());

	EXPECT_TRUE(this->tmp116.setHighLimit(-10.0f));
}

TEST_F(TMP116_Test, setHighLimitReturnsFalseWhenI2CWriteFails) {
	this->disableI2C();
	EXPECT_FALSE(this->tmp116.setHighLimit(0.0f));
}

TEST_F(TMP116_Test, setLowLimitReturnsTrueOnSuccess) {
	const MemoryAddress lowLimitAddress				 = 0x03u;
	const Register		lowLimitExpectedWrittenValue = 0xFB00u;

	EXPECT_CALL(mockedI2C, write(Eq(this->deviceAddress), Eq(lowLimitAddress), Eq(lowLimitExpectedWrittenValue)))
		.WillOnce(ReturnArg<2>());

	EXPECT_TRUE(this->tmp116.setLowLimit(-10.0f));
}

TEST_F(TMP116_Test, setLowLimitReturnsFalseWhenI2CWriteFails) {
	this->disableI2C();
	EXPECT_FALSE(this->tmp116.setLowLimit(0.0f));
}

// --------------------------------------------------------------------------
// Compile-time concept satisfaction checks
// --------------------------------------------------------------------------

// Types that satisfy std::invocable<AlertType> — these must compile.
static_assert(std::invocable<void (*)(TMP116::AlertType), TMP116::AlertType>, "Function pointer must satisfy std::invocable<AlertType>");

struct ValidFunctor {
	void operator()(TMP116::AlertType) {}
};
static_assert(std::invocable<ValidFunctor, TMP116::AlertType>, "Functor must satisfy std::invocable<AlertType>");

// A type that does NOT satisfy the concept — verified at compile time.
struct NonCallable {};
static_assert(!std::invocable<NonCallable, TMP116::AlertType>, "NonCallable must not satisfy std::invocable<AlertType>");
// The following line, if uncommented, must fail to compile (concept constraint not satisfied):
// NonCallable nc; this->tmp116.setAlertCallback(nc);

// --------------------------------------------------------------------------
// Alert callback tests
// --------------------------------------------------------------------------

TEST_F(TMP116_Test, checkAlertDispatchesHighCallbackWhenHighFlagSet) {
	// Config register with highAlertFlag bit (bit 15) set
	EXPECT_CALL(mockedI2C, read(Eq(this->deviceAddress), Eq(MemoryAddress{0x01u}))).WillOnce(Return(Register{0x8220u}));

	TMP116::AlertType received = TMP116::AlertType::Low;
	auto			  cb	   = [&received](TMP116::AlertType type) { received = type; };
	this->tmp116.setAlertCallback(cb);
	this->tmp116.checkAlert();

	EXPECT_EQ(received, TMP116::AlertType::High);
}

TEST_F(TMP116_Test, checkAlertDispatchesLowCallbackWhenLowFlagSet) {
	// Config register with lowAlertFlag bit (bit 14) set
	EXPECT_CALL(mockedI2C, read(Eq(this->deviceAddress), Eq(MemoryAddress{0x01u}))).WillOnce(Return(Register{0x4220u}));

	TMP116::AlertType received = TMP116::AlertType::High;
	auto			  cb	   = [&received](TMP116::AlertType type) { received = type; };
	this->tmp116.setAlertCallback(cb);
	this->tmp116.checkAlert();

	EXPECT_EQ(received, TMP116::AlertType::Low);
}

TEST_F(TMP116_Test, checkAlertDispatchesBothCallbacksWhenBothFlagsSet) {
	// Config register with both highAlertFlag (bit 15) and lowAlertFlag (bit 14) set
	EXPECT_CALL(mockedI2C, read(Eq(this->deviceAddress), Eq(MemoryAddress{0x01u}))).WillOnce(Return(Register{0xC220u}));

	int  callCount = 0;
	auto cb		   = [&callCount](TMP116::AlertType) { callCount++; };
	this->tmp116.setAlertCallback(cb);
	this->tmp116.checkAlert();

	EXPECT_EQ(callCount, 2);
}

TEST_F(TMP116_Test, checkAlertDoesNotDispatchWhenNoFlagsSet) {
	EXPECT_CALL(mockedI2C, read(Eq(this->deviceAddress), Eq(MemoryAddress{0x01u}))).WillOnce(Return(Register{0x0220u}));

	int  callCount = 0;
	auto cb		   = [&callCount](TMP116::AlertType) { callCount++; };
	this->tmp116.setAlertCallback(cb);
	this->tmp116.checkAlert();

	EXPECT_EQ(callCount, 0);
}

TEST_F(TMP116_Test, checkAlertDoesNothingWhenNoCallbackRegistered) {
	// No I2C call expected when no callback is set
	EXPECT_CALL(mockedI2C, read).Times(0);
	this->tmp116.checkAlert();
}

TEST_F(TMP116_Test, checkAlertDoesNothingWhenI2CFails) {
	this->disableI2C();

	int  callCount = 0;
	auto cb		   = [&callCount](TMP116::AlertType) { callCount++; };
	this->tmp116.setAlertCallback(cb);
	this->tmp116.checkAlert();

	EXPECT_EQ(callCount, 0);
}

TEST_F(TMP116_Test, checkAlertCallbackReceivesCorrectAlertType) {
	// Verify the AlertType enum value is forwarded correctly to the callback
	EXPECT_CALL(mockedI2C, read(Eq(this->deviceAddress), Eq(MemoryAddress{0x01u}))).WillOnce(Return(Register{0x8220u}));

	TMP116::AlertType received = TMP116::AlertType::Low;
	auto			  cb	   = [&received](TMP116::AlertType t) { received = t; };
	this->tmp116.setAlertCallback(cb);
	this->tmp116.checkAlert();

	EXPECT_EQ(received, TMP116::AlertType::High);
}

TEST_F(TMP116_Test, setAlertCallbackAcceptsFunctorType) {
	// Demonstrates that a functor (not just a lambda) satisfies the concept constraint
	EXPECT_CALL(mockedI2C, read(Eq(this->deviceAddress), Eq(MemoryAddress{0x01u}))).WillOnce(Return(Register{0x8220u}));

	int			  callCount = 0;
	ValidFunctor  f{};
	this->tmp116.setAlertCallback(f);
	this->tmp116.checkAlert();
	// ValidFunctor::operator() is a no-op; just confirm it compiles and does not crash
}

TEST_F(TMP116_Test, setAlertCallbackClearsCallbackWhenPassedNullptr) {
	// Set a handler, then clear it — checkAlert must not read I2C
	int  callCount = 0;
	auto cb		   = [&callCount](TMP116::AlertType) { callCount++; };
	this->tmp116.setAlertCallback(cb);
	this->tmp116.setAlertCallback(nullptr);

	EXPECT_CALL(mockedI2C, read).Times(0);
	this->tmp116.checkAlert();
	EXPECT_EQ(callCount, 0);
}
