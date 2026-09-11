cmake_minimum_required(VERSION 3.24)

project(TMP116)

set(LIBRARY ${PROJECT_NAME})

enable_language(CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Au (header-only) provides the type-safe physical units of the temperature API, so the driver itself depends on it.
set(AU_EXCLUDE_GTEST_DEPENDENCY ON CACHE BOOL "" FORCE)
FetchContent_Declare(
	au
	GIT_REPOSITORY https://github.com/aurora-opensource/au
	GIT_TAG        0.6.0
	GIT_SHALLOW    TRUE
	FIND_PACKAGE_ARGS 0.6.0 NAMES Au
)
FetchContent_MakeAvailable(au)

add_library(${LIBRARY} STATIC
	Src/TMP116.cpp
)

target_include_directories(${LIBRARY} PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/Inc
)

target_link_libraries(${LIBRARY} PUBLIC
	Au::au
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

	# Host-runnable examples, built by default only when TMP116 is the top-level project.
	option(TMP116_BUILD_EXAMPLES "Build the TMP116 examples" ${PROJECT_IS_TOP_LEVEL})

	if(TMP116_BUILD_EXAMPLES)
		add_executable(${LIBRARY}_Example_ReadTemperature
			Examples/ReadTemperature.cpp
		)

		target_link_libraries(${LIBRARY}_Example_ReadTemperature PRIVATE
			${LIBRARY}::${LIBRARY}
		)

		# Run the example under CTest so it keeps compiling and working as the driver evolves.
		add_test(NAME ${LIBRARY}_Example_ReadTemperature COMMAND ${LIBRARY}_Example_ReadTemperature)
	endif()

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
