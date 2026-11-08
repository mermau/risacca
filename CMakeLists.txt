cmake_minimum_required(VERSION 3.22)
project(RISACCA VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

# --- JUCE: cartella locale (JUCE_DIR) oppure download automatico ----------------------
set(JUCE_DIR "" CACHE PATH "Percorso di una copia locale di JUCE (vuoto = download)")
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} juce EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# --- formati ------------------------------------------------------------------------------
set(RISACCA_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND RISACCA_FORMATS AU)
endif()

juce_add_plugin(Risacca
    COMPANY_NAME "Collateral Effects"
    PRODUCT_NAME "RISACCA"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.png"
    MICROPHONE_PERMISSION_ENABLED TRUE        # macOS: senza, la Standalone riceve solo silenzio dalla scheda audio
    MICROPHONE_PERMISSION_TEXT "RISACCA usa l'ingresso della scheda audio per elaborare il segnale della chitarra."
    BUNDLE_ID "it.mminstruments.risacca"
    PLUGIN_MANUFACTURER_CODE Mmin
    PLUGIN_CODE Rsca
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Distortion
    AU_MAIN_TYPE kAudioUnitType_MusicEffect
    FORMATS ${RISACCA_FORMATS})

target_sources(Risacca PRIVATE
    plugin/PluginProcessor.cpp
    plugin/PluginEditor.cpp
    engine/Circuit.cpp
    engine/SwashEngine.cpp)

target_include_directories(Risacca PRIVATE plugin engine)
# il motore e' codice C++ standard indipendente da JUCE: niente avvisi "stretti" di JUCE
set_source_files_properties(engine/Circuit.cpp engine/SwashEngine.cpp PROPERTIES COMPILE_OPTIONS "-w")

target_compile_definitions(Risacca PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Risacca
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# --- test locali con il processore e l'editor (screenshot della GUI) ---------------------
if(RISACCA_LOCAL_TESTS)
    juce_add_console_app(risacca_plugin_test)
    target_sources(risacca_plugin_test PRIVATE tests/plugin_test.cpp)
    target_include_directories(risacca_plugin_test PRIVATE plugin engine)
    target_compile_definitions(risacca_plugin_test PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(risacca_plugin_test PRIVATE Risacca juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp)
endif()

# --- test offline del motore (senza JUCE) ------------------------------------------------
option(RISACCA_BUILD_TESTS "Compila il test di rendering offline" ON)
if(RISACCA_BUILD_TESTS)
    add_executable(risacca_render tests/render.cpp engine/Circuit.cpp engine/SwashEngine.cpp)
    if(NOT MSVC)
        target_compile_options(risacca_render PRIVATE -O3)
    endif()
endif()
