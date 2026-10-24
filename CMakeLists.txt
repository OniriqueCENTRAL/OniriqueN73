cmake_minimum_required(VERSION 3.22)

# Mac : binaire universel (Intel + Apple Silicon), à définir avant project()
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")

project(OniriqueN73 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.6
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

option(N73_PLUGIN "Compiler le plugin" ON)
option(N73_TESTS "Compiler le banc de mesure" ON)
# Licence ONIRIQUE : toujours dans les builds publiés. OFF seulement pour un build d'essai local.
option(N73_LICENCE "Brancher la licence ONIRIQUE" ON)

if(N73_PLUGIN)
    set(N73_FORMATS VST3)
    if(APPLE)
        list(APPEND N73_FORMATS AU)
    endif()

    juce_add_plugin(OniriqueN73
        COMPANY_NAME                Onirique
        PLUGIN_MANUFACTURER_CODE    Onir
        PLUGIN_CODE                 On73
        FORMATS                     ${N73_FORMATS}
        AU_MAIN_TYPE                kAudioUnitType_Effect
        PRODUCT_NAME                "ONIRIQUE N73"
        IS_SYNTH                    FALSE
        NEEDS_MIDI_INPUT            FALSE
        NEEDS_MIDI_OUTPUT           FALSE
        IS_MIDI_EFFECT              FALSE
        VST3_CATEGORIES             Fx EQ
        COPY_PLUGIN_AFTER_BUILD     FALSE)

    target_sources(OniriqueN73 PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

    file(GLOB N73_KNOB_PNGS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/knobs/*.png")
    juce_add_binary_data(N73Assets SOURCES Resources/logo.png ${N73_KNOB_PNGS})
    set_target_properties(N73Assets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

    target_include_directories(OniriqueN73 PRIVATE Source)

    if(N73_LICENCE)
        include(License/OniriqueLicense.cmake)
        onirique_add_license(OniriqueN73)
        target_compile_definitions(OniriqueN73 PRIVATE N73_LICENCE=1)
    else()
        # les polices de la charte viennent aussi du module de licence
        juce_add_binary_data(OniriqueN73LicenseData
            HEADER_NAME OniriqueLicenseData.h
            NAMESPACE OniriqueLicenseData
            SOURCES License/Fonts/Unbounded800.ttf License/Fonts/HankenGrotesk600.ttf
                    License/Fonts/JetBrainsMono500.ttf License/Fonts/JetBrainsMono600.ttf)
        set_target_properties(OniriqueN73LicenseData PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
        target_link_libraries(OniriqueN73 PRIVATE OniriqueN73LicenseData)
        target_compile_definitions(OniriqueN73 PRIVATE N73_LICENCE=0)
    endif()

    target_compile_definitions(OniriqueN73 PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    if(MSVC)
        target_compile_options(OniriqueN73 PRIVATE /utf-8)
    endif()
    if(APPLE)
        target_compile_definitions(OniriqueN73 PUBLIC JUCE_COREGRAPHICS_RENDER_WITH_MULTIPLE_PAINT_CALLS=1)
    endif()

    target_link_libraries(OniriqueN73
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
            N73Assets
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()

# Banc de mesure du moteur (console) : réponses, distorsion, repliement, CPU, latence, robustesse
if(N73_TESTS)
    juce_add_console_app(N73Bench PRODUCT_NAME "N73Bench")
    target_sources(N73Bench PRIVATE Tests/N73Bench.cpp)
    target_include_directories(N73Bench PRIVATE Source)
    target_compile_definitions(N73Bench PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    if(MSVC)
        target_compile_options(N73Bench PRIVATE /utf-8)
    endif()
    target_link_libraries(N73Bench PRIVATE juce::juce_dsp juce::juce_audio_basics
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
