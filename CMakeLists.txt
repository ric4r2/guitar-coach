cmake_minimum_required(VERSION 3.22)
project(GuitarCoach VERSION 0.1.0)

# JUCE 8.0.6. If GuitarDrive has already downloaded it next door, use that copy instead of downloading again.
set(_GD_JUCE "${CMAKE_CURRENT_SOURCE_DIR}/../GuitarDrive/build/_deps/juce-src")
if(NOT FETCHCONTENT_SOURCE_DIR_JUCE AND EXISTS "${_GD_JUCE}/CMakeLists.txt")
    set(FETCHCONTENT_SOURCE_DIR_JUCE "${_GD_JUCE}" CACHE PATH "Local JUCE source" FORCE)
    message(STATUS "Using JUCE from ${_GD_JUCE}")
endif()

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

# Lesson text and exercise settings, embedded in the program.
juce_add_binary_data(GuitarCoachContent SOURCES Content/lessons.json Content/licks.json)

# Everything that is not UI: music theory, pitch detection, synth voices, lessons and progress.
set(CORE_SOURCES
    Source/core/Theory.cpp
    Source/core/Fingerings.cpp
    Source/core/BackingTrack.cpp
    Source/core/Licks.cpp
    Source/core/Rhythm.cpp
    Source/core/Planner.cpp
    Source/core/InputAnalyzer.cpp
    Source/core/Lessons.cpp
    Source/core/Progress.cpp)

set(UI_SOURCES
    Source/audio/AudioEngine.cpp
    Source/ui/StaffView.cpp
    Source/ui/FretboardView.cpp
    Source/ui/ExerciseView.cpp
    Source/ui/TheoryPage.cpp
    Source/ui/PracticePage.cpp
    Source/ui/ImprovPage.cpp
    Source/ui/StatsPage.cpp
    Source/ui/MainComponent.cpp)

juce_add_gui_app(GuitarCoach
    COMPANY_NAME "Joe Garcia"
    PRODUCT_NAME "GuitarCoach"
    BUNDLE_ID com.joegarcia.guitarcoach
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png"
    MICROPHONE_PERMISSION_ENABLED TRUE)

target_sources(GuitarCoach PRIVATE Source/Main.cpp ${CORE_SOURCES} ${UI_SOURCES})
target_include_directories(GuitarCoach PRIVATE Source)
target_compile_definitions(GuitarCoach PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:GuitarCoach,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:GuitarCoach,JUCE_VERSION>")

# Optional ASIO (lowest latency with the Audient driver): -DASIO_SDK_DIR=<folder containing common/>.
# The SDK is not redistributable, so it stays outside the repo. GuitarDrive's copy is used when present.
set(_GD_ASIO "${CMAKE_CURRENT_SOURCE_DIR}/../_sdk/ASIO-SDK_2.3.4/ASIOSDK")
if(NOT ASIO_SDK_DIR AND EXISTS "${_GD_ASIO}/common/asio.h")
    set(ASIO_SDK_DIR "${_GD_ASIO}" CACHE PATH "Steinberg ASIO SDK root" FORCE)
endif()
set(ASIO_SDK_DIR "${ASIO_SDK_DIR}" CACHE PATH "Steinberg ASIO SDK root (the folder containing common/)")
if(ASIO_SDK_DIR AND EXISTS "${ASIO_SDK_DIR}/common/asio.h")
    message(STATUS "ASIO enabled: ${ASIO_SDK_DIR}")
    set(COACH_ASIO_DEFS JUCE_ASIO=1)
    set(COACH_ASIO_INC "${ASIO_SDK_DIR}/common")
endif()
target_compile_definitions(GuitarCoach PRIVATE ${COACH_ASIO_DEFS})
target_include_directories(GuitarCoach PRIVATE ${COACH_ASIO_INC})

target_link_libraries(GuitarCoach PRIVATE
    GuitarCoachContent
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# Tests: cmake -DGUITARCOACH_BUILD_TESTS=ON, then build GuitarCoachTests and GuitarCoachSnapshot.
option(GUITARCOACH_BUILD_TESTS "Build the offline tests and the screen snapshot tool" OFF)
if(GUITARCOACH_BUILD_TESTS)
    juce_add_console_app(GuitarCoachTests PRODUCT_NAME "GuitarCoachTests")
    target_sources(GuitarCoachTests PRIVATE Tests/CoreTests.cpp ${CORE_SOURCES})
    target_include_directories(GuitarCoachTests PRIVATE Source)
    target_compile_definitions(GuitarCoachTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(GuitarCoachTests PRIVATE
        GuitarCoachContent
        juce::juce_core juce::juce_events
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

    # renders every screen to PNG files, to check the look without opening the program
    juce_add_console_app(GuitarCoachSnapshot PRODUCT_NAME "GuitarCoachSnapshot")
    target_sources(GuitarCoachSnapshot PRIVATE Tests/Snapshot.cpp ${CORE_SOURCES} ${UI_SOURCES})
    target_include_directories(GuitarCoachSnapshot PRIVATE Source ${COACH_ASIO_INC})
    target_compile_definitions(GuitarCoachSnapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 ${COACH_ASIO_DEFS})
    target_link_libraries(GuitarCoachSnapshot PRIVATE
        GuitarCoachContent
        juce::juce_audio_utils
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
