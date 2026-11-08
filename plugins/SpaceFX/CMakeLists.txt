nwa_claim_plugin_identity(SpaceFX Spf1 "SPACE-FX")

juce_add_plugin(SpaceFX
    COMPANY_NAME "NEOWAVE AUDIO"
    VERSION ${PROJECT_VERSION}
    PLUGIN_MANUFACTURER_CODE Neow
    PLUGIN_CODE Spf1
    FORMATS VST3 Standalone
    PRODUCT_NAME "SPACE-FX"
    BUNDLE_ID com.neowaveaudio.spacefx
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Delay Reverb)

target_sources(SpaceFX PRIVATE Source/SpaceProcessor.cpp Source/gui/SpaceEditor.cpp)
target_include_directories(SpaceFX PRIVATE Source)
target_compile_definitions(SpaceFX PUBLIC SPACE_WITH_GUI=1)
target_link_libraries(SpaceFX
    PRIVATE nwa_core
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# DSP-Tests (reines C++, kein JUCE)
add_executable(SpaceTest Tests/SpaceTest.cpp)
target_include_directories(SpaceTest PRIVATE Source)
target_compile_features(SpaceTest PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(SpaceTest PRIVATE /W4 /O2)
else()
    target_compile_options(SpaceTest PRIVATE -Wall -Wextra -O2)
endif()
add_test(NAME space_dsp COMMAND SpaceTest)

juce_add_console_app(SpaceProcessorTest PRODUCT_NAME "SpaceProcessorTest")
target_sources(SpaceProcessorTest PRIVATE Tests/ProcessorTest.cpp Source/SpaceProcessor.cpp)
target_include_directories(SpaceProcessorTest PRIVATE Source)
target_compile_definitions(SpaceProcessorTest PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=0 SPACE_WITH_GUI=0)
target_link_libraries(SpaceProcessorTest PRIVATE nwa_core PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
add_test(NAME space_processor COMMAND SpaceProcessorTest)

# GUI-Schnappschuss + Bedienungs-Checks (nur Linux/xvfb im CI)
juce_add_console_app(SpaceShot PRODUCT_NAME "SpaceShot")
target_sources(SpaceShot PRIVATE Tests/EditorShot.cpp Source/SpaceProcessor.cpp Source/gui/SpaceEditor.cpp)
target_include_directories(SpaceShot PRIVATE Source)
target_compile_definitions(SpaceShot PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1 SPACE_WITH_GUI=1)
target_link_libraries(SpaceShot PRIVATE nwa_core PUBLIC juce::juce_recommended_config_flags)
if(UNIX AND NOT APPLE)
    add_test(NAME space_gui COMMAND SpaceShot ${CMAKE_BINARY_DIR}/space_shots)
endif()
