cmake_minimum_required(VERSION 3.22)
project(pnvTune VERSION 0.4.1 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/JUCE-8.0.9/CMakeLists.txt")
    add_subdirectory(vendor/JUCE-8.0.9 EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()
juce_add_plugin(pnvTune
    COMPANY_NAME "PNV Audio" BUNDLE_ID "com.pnvaudio.pnvtune"
    PLUGIN_MANUFACTURER_CODE Prem PLUGIN_CODE Ptun
    FORMATS AU VST3 Standalone PRODUCT_NAME "pnvTune"
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE COPY_PLUGIN_AFTER_BUILD FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "pnvTune uses your microphone for live vocal pitch correction."
    AU_MAIN_TYPE kAudioUnitType_Effect VST3_CATEGORIES Fx Pitch\ Shift)
target_sources(pnvTune PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(pnvTune PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(pnvTune PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)
add_executable(PremTuneDSPTests tests/DSPTests.cpp)
target_include_directories(PremTuneDSPTests PRIVATE Source)
enable_testing()
add_test(NAME DSP COMMAND PremTuneDSPTests)
add_executable(PremTuneChecks)
set_target_properties(PremTuneChecks PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/PremTuneChecks_artefacts/Release")
target_sources(PremTuneChecks PRIVATE tests/PluginChecks.cpp)
target_include_directories(PremTuneChecks PRIVATE Source)
target_link_libraries(PremTuneChecks PRIVATE pnvTune juce::juce_audio_utils)
