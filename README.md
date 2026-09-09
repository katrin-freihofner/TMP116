# TMP116 Digital Temperature Sensor

[![License](https://img.shields.io/github/license/LawrenceStanton/TMP116)](LICENCE.md)
[![CTest](https://github.com/LawrenceStanton/TMP116/actions/workflows/ctest.yml/badge.svg)](https://github.com/LawrenceStanton/TMP116/actions/workflows/ctest.yml)
[![CodeQL](https://github.com/LawrenceStanton/TMP116/actions/workflows/codeql.yml/badge.svg)](https://github.com/LawrenceStanton/TMP116/actions/workflows/codeql.yml)

## Overview

The TMP116 is an embedded temperature sensor, produced by Texas Instruments, and operates over a SMBus digital communications interface. This driver provides a simple C++ object-orientated driver to allow for control and data reception from this device, and is agnostic to any embedded platform.

The design philosophy of this driver is comparatively unique in the embedded systems space. The object orientated approach results in a simple, scalable, and extensible driver, which is easy to use and understand. The driver is also designed to be agnostic to any embedded platform, and is therefore portable to any embedded system, with any I2C implementation (see [Design Patterns](#design-patterns)). The system also adopts strong unit testing with the [Google Test](https://google.github.io/googletest/) framework, allowing for a high degree of confidence in the driver's operation and ease of further development. CMake also allows for easy integration into parent projects.

## How to Use

1. Add this project as a `git submodule`.

    ```zsh
    git submodule add https://github.com/LawrenceStanton/TMP116 Modules/TMP116
    ```

2. Build with CMake by adding as a subdirectory.

    ```cmake
    # Add to Your Top-Level CMakeLists.txt
    add_subdirectory(Modules/TMP116)
    # ...
    target_link_libraries(${YOUR_EXECUTABLE_NAME} PRIVATE 
        TMP116::TMP116
    )    
    ```

    > If you are not using CMake, you must then simply add the HDC1080 include directory and source files to your chosen build system.

3. Provide an I2C interface when constructing the driver. Refer to [Design Patterns](#design-patterns) below and [TMP116.hpp](Inc/TMP116.hpp) for more information.

4. Construct the TMP116 I2C interface and class object.

    ```cpp
    // Construct the I2C interface
    MyI2C i2cInterface(myParams);
    // Construct the HDC1080 class object
    TMP116::TMP116 sensor(&i2cInterface);
    ```

## Design Patterns

This driver follows an [Strategy Design Pattern](https://en.wikipedia.org/wiki/Strategy_pattern) with regards to the I2C communication. The driver defines an I2C interface (`TMP116::I2C`). The user must then provide a concrete implementation of this interface, and provide it to the driver class.

Often a concrete implementation will simply translate the I2C operations to the embedded platform's Hardware Abstraction Layer (HAL). For example, the [STM32Cube HAL](https://www.st.com/en/embedded-software/stm32cube-mcu-mpu-packages.html) provides an I2C interface, which can be used to implement the I2C operations. However, the user may also provide their own low level implementation, which may be useful in some applications, or mocked implementation, which may be useful for testing purposes (see [Testing](#testing)).

### Example

Below is an example of a typical declaration of the I2C interface.

```cpp
#include "TMP116.hpp"

class MyI2C : public TMP116::I2C {
public:
    MyI2C(MyI2CParams params) : params(params) {}
    virtual ~MyI2C() {}

    virtual std::optional<TMP116::I2C::Register> read(TMP116::I2C::MemoryAddress address) override;
    virtual std::optional<TMP116::I2C::Register> write(TMP116::I2C::MemoryAddress address, TMP116::I2C::Register) override;
}

// Definitions made in a separate source file.
```

Refer to [Examples] for concrete examples of this design pattern.

## Temperatures

Temperatures are physical quantities, not bare `float`s. The limit setters take any point on the
temperature scale, and `getTemperature` returns one, so a value cannot be silently misread as the
wrong unit on its way between the driver, a controller and a display:

```cpp
using namespace mp_units;

// The same threshold, in whichever unit is natural at the call site. No manual conversion, and
// both write the same register value. The setters are [[nodiscard]]: the returned optional is
// empty if the I2C write failed.
const auto written = sensor.setHighLimit(point<si::degree_Celsius>(30.0f));
if (!written) reportBusFailure();

(void)sensor.setHighLimit(point<usc::degree_Fahrenheit>(86.0f)); // same register value

// The reading carries its unit, and is converted explicitly where it is needed.
TMP116::Temperature temperature = sensor.getTemperature();
auto fahrenheit = temperature.in(usc::degree_Fahrenheit);
```

These are rejected at compile time rather than at runtime:

```cpp
sensor.setHighLimit(30.0f);                              // a bare float carries no unit
sensor.setHighLimit(point<si::metre>(1.0f));             // not a temperature
sensor.setHighLimit(delta<si::degree_Celsius>(30.0f));   // a difference, not a point on the scale
float t = sensor.getTemperature();                       // the unit cannot be discarded by accident
```

That last distinction is the reason the API uses `quantity_point` rather than `quantity`: for a
scale with an offset zero, a *point* of 86 degF is 30 degC, whereas a *difference* of 86 degF is
47.8 degC. Conflating the two is exactly the class of bug this API removes.

Units come from [mp-units], the reference implementation of [P1935]. The library is header only and
compiles to the same code a `float` would: at `-Os` the driver object has no undefined symbols and
performs no allocation. It requires C++20.

## Temperature Alerts

The TMP116 can assert its ALERT pin when the measured temperature crosses the high or low limit
registers, which avoids polling the I2C bus at a fixed rate and catches fast thermal events between
reads.

Set the limits, then register a callback:

```cpp
sensor.setConfig(TMP116::Config::ThermalAlertModeSelect::ALERT);

using namespace mp_units;
if (!sensor.setHighLimit(point<si::degree_Celsius>(250.0f))) reportBusFailure();
if (!sensor.setLowLimit(point<si::degree_Celsius>(0.0f))) reportBusFailure();

sensor.setAlertCallback([](TMP116::AlertType alertType) {
    if (alertType == TMP116::AlertType::High) shutdownHeatingElement();
});
```

The callback is a plain function pointer rather than a `std::function`, so that registration cannot
heap allocate on a bare-metal target. A captureless lambda converts to it implicitly, as above. To
pass state without a global, use the context overload, whose `void *` is forwarded back unchanged
and is never dereferenced by the driver:

```cpp
sensor.setAlertCallback(
    [](TMP116::AlertType alertType, void *context) { static_cast<Oven *>(context)->onAlert(alertType); },
    &oven
);
```

The context is owned by the caller. Call `clearAlertCallback()` before destroying it.

### Servicing the alert

The driver is platform agnostic and contains no GPIO handling, so it cannot observe the ALERT pin
itself. The application services the pin and calls `serviceAlert()`, which reads the alert flags and
dispatches the callback:

```cpp
void onAlertPinInterrupt(void) {
    sensor.serviceAlert(); // Consider deferring to a task if your I2C driver blocks.
}
```

If both flags are set, the callback is invoked twice, once per `AlertType`. In `ALERT` mode reading
the configuration register clears the flags, so a given crossing is reported once. `serviceAlert()`
returns the `Config` it read, or `std::nullopt` if the I2C read failed.

## Testing

This driver is unit tested using the GoogleTest and GoogleMock frameworks. The tests are located in the [Tests](Tests) directory.

The tests are built using CMake. Given the limitations of many embedded systems, the tests are designed to be run on a host machine, and not on the embedded system itself. This is done by checking the CMake variable `CMAKE_CROSSCOMPILING` and only building the tests if this is false. To build the tests, configure your build presets to perform a local build.

> If gtest and gmock are not installed on your system, CMake will attempt to automatically download and build them automatically with FetchContent. Disable this behaviour by setting the CMake option `TMP116_AUTOFETCH_GTEST` to `OFF`.
>
> _Command Line:_
>
>```zsh
>cmake -DTMP116_AUTOFETCH_GTEST=OFF
>```
>
> _Parent CMakeLists.txt:_
>
> ```cmake
> set(TMP116_AUTOFETCH_GTEST OFF)
> # ...
> add_subdirectory(Modules/TMP116)
>```

The tests can then be run using CTest.

```zsh
mkdir Build && cd Build
cmake ..
cmake --build .
ctest
```

Run tests automatically be setting `test` as a build target in your presets.

These tests will be included in the parent build if ctest is also used there.

[mp-units]: https://github.com/mpusz/mp-units
[P1935]: https://mpusz.github.io/wg21-papers/papers/1935R0_a_cpp_approach_to_physical_units.html
