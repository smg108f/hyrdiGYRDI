cmake_minimum_required(VERSION 3.22)

# Universal binary (Apple Silicon + Intel) unless you override it
if(APPLE AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
endif()
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

project(hyrdiGYRDI VERSION 0.2.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- JUCE ---------------------------------------------------------------------
# Either point at a local checkout (-DHYRDI_JUCE_DIR=/path/to/JUCE) or let CMake fetch it.
set(HYRDI_JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (optional)")
if(HYRDI_JUCE_DIR)
    add_subdirectory(${HYRDI_JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.9
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# --- plugin -------------------------------------------------------------------
set(HYRDI_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND HYRDI_FORMATS AU)
endif()

juce_add_plugin(hyrdiGYRDI
    PRODUCT_NAME                "hyrdiGYRDI"
    COMPANY_NAME                "voluntyr"   # plain ASCII for OS/host metadata; the UI shows "völuntyr"
    BUNDLE_ID                   "com.raidoradio.hyrdigyrdi"
    PLUGIN_MANUFACTURER_CODE    Rdio
    PLUGIN_CODE                 Hgyr
    FORMATS                     ${HYRDI_FORMATS}
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Instrument Synth
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD     TRUE)

target_sources(hyrdiGYRDI PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Params.cpp)

target_compile_definitions(hyrdiGYRDI PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(hyrdiGYRDI
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# --- offline render test (optional) -------------------------------------------
# Renders a short performance to WAV without a DAW:  cmake -DHYRDI_BUILD_TESTS=ON ...
option(HYRDI_BUILD_TESTS "Build the offline render test" OFF)
if(HYRDI_BUILD_TESTS)
    juce_add_console_app(hyrdiRender PRODUCT_NAME "hyrdiRender")
    target_sources(hyrdiRender PRIVATE tests/RenderTest.cpp Source/Params.cpp)
    target_include_directories(hyrdiRender PRIVATE Source)
    target_compile_definitions(hyrdiRender PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(hyrdiRender PRIVATE juce::juce_audio_formats juce::juce_audio_processors
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
