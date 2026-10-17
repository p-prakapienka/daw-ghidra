cmake_minimum_required(VERSION 3.22)

project(daw_ghidra VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies are fetched with CPM, as in RstSynth.
file(
        DOWNLOAD
        https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.40.2/CPM.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake
        EXPECTED_HASH SHA256=c8cdc32c03816538ce22781ed72964dc864b2a34a310d3b7104812a5ca2d835d
)
include(${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)

# SoundTouch has no GitHub home, so it is fetched by URL. The tag is a cache
# variable because upstream tagging is easy to get wrong from memory.
set(SOUNDTOUCH_GIT_TAG "2.3.3" CACHE STRING "SoundTouch release to build against")

CPMAddPackage(
        NAME SoundTouch
        GIT_REPOSITORY https://codeberg.org/soundtouch/soundtouch.git
        GIT_TAG ${SOUNDTOUCH_GIT_TAG}
)

option(DAW_GHIDRA_BUILD_TESTS "Build the engine tests" ON)

if (DAW_GHIDRA_BUILD_TESTS)
    CPMAddPackage(
            NAME googletest
            GITHUB_REPOSITORY google/googletest
            GIT_TAG v1.17.0
            OPTIONS
            "INSTALL_GTEST OFF"
            "gtest_force_shared_crt ON"
    )
endif ()

# Reconstructed engine code, by role:
#   core/              Machine, the base of every processor
#   dsp/               envelopes, LFO, oscillator, filters, control voltage
#   ui/                the value side of UI classes the engine depends on
#   sequencer/         sequencer events
#   machines/subsynth/ SubSynth and its parts
# Classes ported to the original's structure say so in their header comment;
# the others reproduce the behaviour through an API of our own.
add_library(engine STATIC
        src/dsp/ADSR.cpp
        src/dsp/ControlVoltage.cpp
        src/dsp/FixedPointSVFilter.cpp
        src/dsp/LFO.cpp
        src/dsp/Oscillator.cpp
        src/ui/CustomWaveform.cpp
        src/ui/Keyboard.cpp
        src/machines/subsynth/SubSynth.cpp
        src/machines/subsynth/SubSynthArchive.cpp
        src/machines/subsynth/SubSynthControls.cpp
        src/machines/subsynth/SubSynthVoice.cpp
)

target_include_directories(engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(engine PUBLIC cxx_std_20)

# Bench executable for listening to and plotting the engine.
add_executable(daw_ghidra
        main.cpp
)

# AAFilter.h lives under soundtouch/source/SoundTouch and is not installed as a
# public header, so the internal source directory goes on the include path.
target_include_directories(daw_ghidra PRIVATE ${SoundTouch_SOURCE_DIR}/source/SoundTouch)
target_link_libraries(daw_ghidra PRIVATE engine SoundTouch)

if (DAW_GHIDRA_BUILD_TESTS)
    enable_testing()

    add_executable(engine_tests
            tests/dsp/ADSRTest.cpp
            tests/dsp/ControlVoltageTest.cpp
            tests/dsp/FixedPointSVFilterTest.cpp
            tests/dsp/LFOTest.cpp
            tests/dsp/OscillatorTest.cpp
            tests/ui/CustomWaveformTest.cpp
            tests/machines/subsynth/SubSynthArchiveTest.cpp
            tests/machines/subsynth/SubSynthControlsTest.cpp
            tests/machines/subsynth/SubSynthTest.cpp
            tests/machines/subsynth/SubSynthVoiceTest.cpp
    )

    target_link_libraries(engine_tests PRIVATE engine GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(engine_tests)
endif ()
