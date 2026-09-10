cmake_minimum_required(VERSION 3.25) # mp-units requires at least 3.25.

project(TMP116)

set(LIBRARY ${PROJECT_NAME})

enable_language(CXX)

set(CMAKE_CXX_STANDARD 20) # mp-units v2 is built on C++20 concepts.
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Physical units for the temperature API, so that quantities carry their unit in the type system.
# Contract checking is disabled: the default (GSL-LITE) adds a dependency and a throwing check
# layer, neither of which suits a bare metal target.
set(MP_UNITS_API_CONTRACTS NONE CACHE STRING "" FORCE)
set(MP_UNITS_BUILD_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
	mp-units
	GIT_REPOSITORY https://github.com/mpusz/mp-units.git
	GIT_TAG        v2.5.0
	SOURCE_SUBDIR  src
)
FetchContent_MakeAvailable(mp-units)

add_library(${LIBRARY} STATIC
	Src/TMP116.cpp
)

# PUBLIC: the unit types appear in the public header, so consumers need them too.
target_link_libraries(${LIBRARY} PUBLIC
	mp-units::mp-units
)

target_include_directories(${LIBRARY} PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/Inc
)

add_library(${LIBRARY}::${LIBRARY} ALIAS ${LIBRARY})

if(NOT CMAKE_CROSSCOMPILING)
	option(TMP116_CODE_COVERAGE "Enable gcovr code coverage for TMP116" OFF)

	FetchContent_Declare(
		googletest
		GIT_REPOSITORY https://github.com/google/googletest
		GIT_TAG        origin/main
	)
	set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
	FetchContent_MakeAvailable(googletest)
	enable_testing()

	set(TEST_EXECUTABLE ${LIBRARY}_Test)

	add_executable(${TEST_EXECUTABLE}
		Test/TMP116.test.cpp
		Test/TMP116_Config.test.cpp
	)

	target_compile_options(${TEST_EXECUTABLE} PRIVATE
		$<$<BOOL:${TMP116_CODE_COVERAGE}>:--coverage>
	)

	target_link_libraries(${TEST_EXECUTABLE} PRIVATE
		${LIBRARY}::${LIBRARY}
		GTest::gtest_main
		GTest::gmock
	)

	target_link_options(${TEST_EXECUTABLE} PRIVATE
		$<$<BOOL:${TMP116_CODE_COVERAGE}>:--coverage>
	)

	include(GoogleTest)
	gtest_discover_tests(${TEST_EXECUTABLE})

	if(TMP116_CODE_COVERAGE)
		set(GCOVR_COMMAND gcovr --root ${CMAKE_SOURCE_DIR} --filter '.*/TMP116/.*' --exclude '.*\.test\..*' ${CMAKE_CURRENT_BINARY_DIR})
		set(SILENT_RUN_COMMAND ./${TEST_EXECUTABLE} > /dev/null)

		add_custom_command(TARGET ${TEST_EXECUTABLE} PRE_BUILD
			COMMENT "Cleaning previous gcov data files for ADS124S08 Tests..."
			COMMAND find ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${TEST_EXECUTABLE}.dir -type f -name "*.gcda" -delete
		)

		add_custom_command(TARGET ${TEST_EXECUTABLE} POST_BUILD
			COMMENT "Running gcovr Code Coverage for TMP116 Tests..."
			COMMAND ${SILENT_RUN_COMMAND} && ${GCOVR_COMMAND} || echo "Code coverage failed. All tests must pass for code coverage to run."
		)
	endif()
endif()
