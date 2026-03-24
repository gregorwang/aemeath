cmake_minimum_required(VERSION 3.24)

project(CyberCompanionCpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

include(CTest)

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Widgets Network Multimedia Test)

qt_standard_project_setup()

set(AEMEATH_CPP_SOURCES
    src_cpp/main.cpp
    src_cpp/app/application_bootstrap.cpp
    src_cpp/app/application_bootstrap.h
    src_cpp/runtime/app_logger.cpp
    src_cpp/runtime/app_logger.h
    src_cpp/runtime/app_paths.cpp
    src_cpp/runtime/app_paths.h
    src_cpp/runtime/auto_start_manager.cpp
    src_cpp/runtime/auto_start_manager.h
    src_cpp/runtime/character_asset_catalog.cpp
    src_cpp/runtime/character_asset_catalog.h
    src_cpp/runtime/character_manifest_catalog.cpp
    src_cpp/runtime/character_manifest_catalog.h
    src_cpp/runtime/character_switch_matcher.cpp
    src_cpp/runtime/character_switch_matcher.h
    src_cpp/runtime/config_repository.cpp
    src_cpp/runtime/config_repository.h
    src_cpp/runtime/idle_invasion_controller.cpp
    src_cpp/runtime/idle_invasion_controller.h
    src_cpp/runtime/presence_detector.cpp
    src_cpp/runtime/presence_detector.h
    src_cpp/runtime/runtime_director.cpp
    src_cpp/runtime/runtime_director.h
    src_cpp/runtime/screen_capture.cpp
    src_cpp/runtime/screen_capture.h
    src_cpp/runtime/scripted_trajectory_catalog.cpp
    src_cpp/runtime/scripted_trajectory_catalog.h
    src_cpp/runtime/single_instance_guard.cpp
    src_cpp/runtime/single_instance_guard.h
    src_cpp/runtime/version_manifest.cpp
    src_cpp/runtime/version_manifest.h
    src_cpp/runtime/vision_frame_analyzer.cpp
    src_cpp/runtime/vision_frame_analyzer.h
    src_cpp/runtime/voice_command_matcher.cpp
    src_cpp/runtime/voice_command_matcher.h
    src_cpp/runtime/voice_script_catalog.cpp
    src_cpp/runtime/voice_script_catalog.h
    src_cpp/services/audio_service.cpp
    src_cpp/services/audio_service.h
    src_cpp/services/audio_output_monitor_service.cpp
    src_cpp/services/audio_output_monitor_service.h
    src_cpp/services/fullscreen_monitor_service.cpp
    src_cpp/services/fullscreen_monitor_service.h
    src_cpp/services/hotkey_service_win.cpp
    src_cpp/services/hotkey_service_win.h
    src_cpp/services/idle_monitor_service.cpp
    src_cpp/services/idle_monitor_service.h
    src_cpp/services/openai_compatible_client.cpp
    src_cpp/services/openai_compatible_client.h
    src_cpp/services/openai_commentary_service.cpp
    src_cpp/services/openai_commentary_service.h
    src_cpp/services/service_contracts.h
    src_cpp/services/vision_service.cpp
    src_cpp/services/vision_service.h
    src_cpp/services/voice_input_service.cpp
    src_cpp/services/voice_input_service.h
    src_cpp/ui/entity_widget.cpp
    src_cpp/ui/entity_widget.h
    src_cpp/ui/entity_positions.cpp
    src_cpp/ui/entity_positions.h
    src_cpp/ui/settings_dialog.cpp
    src_cpp/ui/settings_dialog.h
    src_cpp/ui/trajectory_player.cpp
    src_cpp/ui/trajectory_player.h
    src_cpp/ui/tray_controller.cpp
    src_cpp/ui/tray_controller.h
)

qt_add_executable(CyberCompanionCpp WIN32
    ${AEMEATH_CPP_SOURCES}
)

target_include_directories(CyberCompanionCpp PRIVATE src_cpp)

target_link_libraries(CyberCompanionCpp
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
        Qt6::Multimedia
)

if(WIN32)
    target_link_libraries(CyberCompanionCpp PRIVATE Ole32 Uuid)
endif()

target_compile_definitions(CyberCompanionCpp
    PRIVATE
        QT_NO_KEYWORDS
)

set_target_properties(CyberCompanionCpp PROPERTIES
    OUTPUT_NAME "CyberCompanionCpp"
)

install(TARGETS CyberCompanionCpp
    BUNDLE DESTINATION .
    RUNTIME DESTINATION .
)

if(BUILD_TESTING)
    qt_add_executable(CyberCompanionCppConfigTests
        tests_cpp/test_config_repository.cpp
        src_cpp/runtime/config_repository.cpp
        src_cpp/runtime/config_repository.h
    )

    target_include_directories(CyberCompanionCppConfigTests PRIVATE src_cpp)

    target_link_libraries(CyberCompanionCppConfigTests
        PRIVATE
            Qt6::Core
            Qt6::Test
    )

    qt_add_executable(CyberCompanionCppAssetTests
        tests_cpp/test_character_asset_catalog.cpp
        src_cpp/runtime/app_paths.cpp
        src_cpp/runtime/app_paths.h
        src_cpp/runtime/character_asset_catalog.cpp
        src_cpp/runtime/character_asset_catalog.h
    )

    target_include_directories(CyberCompanionCppAssetTests PRIVATE src_cpp)

    target_link_libraries(CyberCompanionCppAssetTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Test
    )

    qt_add_executable(CyberCompanionCppAutoStartTests
        tests_cpp/test_auto_start_manager.cpp
        src_cpp/runtime/auto_start_manager.cpp
        src_cpp/runtime/auto_start_manager.h
    )

    target_include_directories(CyberCompanionCppAutoStartTests PRIVATE src_cpp)

    target_link_libraries(CyberCompanionCppAutoStartTests
        PRIVATE
            Qt6::Core
            Qt6::Test
    )

    qt_add_executable(CyberCompanionCppCharacterManifestTests
        tests_cpp/test_character_manifest_catalog.cpp
        src_cpp/runtime/app_paths.cpp
        src_cpp/runtime/app_paths.h
        src_cpp/runtime/character_manifest_catalog.cpp
        src_cpp/runtime/character_manifest_catalog.h
    )

    target_include_directories(CyberCompanionCppCharacterManifestTests PRIVATE src_cpp)

    target_link_libraries(CyberCompanionCppCharacterManifestTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Test
    )

    qt_add_executable(CyberCompanionCppCharacterSwitchTests
        tests_cpp/test_character_switch_matcher.cpp
        src_cpp/runtime/character_switch_matcher.cpp
        src_cpp/runtime/character_switch_matcher.h
        src_cpp/runtime/character_manifest_catalog.h
    )

    target_include_directories(CyberCompanionCppCharacterSwitchTests PRIVATE src_cpp)

    target_link_libraries(CyberCompanionCppCharacterSwitchTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Test
    )

    qt_add_executable(CyberCompanionCppVoiceScriptTests
        tests_cpp/test_voice_script_catalog.cpp
        src_cpp/runtime/app_paths.cpp
        src_cpp/runtime/app_paths.h
        src_cpp/runtime/voice_script_catalog.cpp
        src_cpp/runtime/voice_script_catalog.h
    )

    target_include_directories(CyberCompanionCppVoiceScriptTests PRIVATE src_cpp)

    target_link_libraries(CyberCompanionCppVoiceScriptTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Test
    )

    qt_add_executable(CyberCompanionCppVoiceCommandTests
        tests_cpp/test_voice_command_matcher.cpp
        src_cpp/runtime/voice_command_matcher.cpp
        src_cpp/runtime/voice_command_matcher.h
    )

    target_include_directories(CyberCompanionCppVoiceCommandTests PRIVATE src_cpp)

    target_link_libraries(CyberCompanionCppVoiceCommandTests
        PRIVATE
            Qt6::Core
            Qt6::Test
    )

    qt_add_executable(CyberCompanionCppVersionTests
        tests_cpp/test_version_manifest.cpp
        src_cpp/runtime/app_paths.cpp
        src_cpp/runtime/app_paths.h
        src_cpp/runtime/version_manifest.cpp
        src_cpp/runtime/version_manifest.h
    )

    target_include_directories(CyberCompanionCppVersionTests PRIVATE src_cpp)

    target_link_libraries(CyberCompanionCppVersionTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Test
    )

    qt_add_executable(CyberCompanionCppPresenceTests
        tests_cpp/test_presence_detector.cpp
        src_cpp/runtime/presence_detector.cpp
        src_cpp/runtime/presence_detector.h
        src_cpp/services/service_contracts.h
    )

    target_include_directories(CyberCompanionCppPresenceTests PRIVATE src_cpp)

    target_link_libraries(CyberCompanionCppPresenceTests
        PRIVATE
            Qt6::Core
            Qt6::Test
    )

    qt_add_executable(CyberCompanionCppVisionAnalyzerTests
        tests_cpp/test_vision_frame_analyzer.cpp
        src_cpp/runtime/vision_frame_analyzer.cpp
        src_cpp/runtime/vision_frame_analyzer.h
        src_cpp/services/service_contracts.h
    )

    target_include_directories(CyberCompanionCppVisionAnalyzerTests PRIVATE src_cpp)

    target_link_libraries(CyberCompanionCppVisionAnalyzerTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Test
    )

    qt_add_executable(CyberCompanionCppTrajectoryCatalogTests
        tests_cpp/test_scripted_trajectory_catalog.cpp
        src_cpp/runtime/app_paths.cpp
        src_cpp/runtime/app_paths.h
        src_cpp/runtime/scripted_trajectory_catalog.cpp
        src_cpp/runtime/scripted_trajectory_catalog.h
    )

    target_include_directories(CyberCompanionCppTrajectoryCatalogTests PRIVATE src_cpp)

    target_link_libraries(CyberCompanionCppTrajectoryCatalogTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Test
    )

    qt_add_executable(CyberCompanionCppUiTests
        tests_cpp/test_entity_positions.cpp
        src_cpp/ui/entity_positions.cpp
        src_cpp/ui/entity_positions.h
    )

    target_include_directories(CyberCompanionCppUiTests PRIVATE src_cpp)

    target_link_libraries(CyberCompanionCppUiTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Test
    )

    qt_add_executable(CyberCompanionCppWidgetTests
        tests_cpp/test_entity_widget.cpp
        src_cpp/runtime/app_paths.cpp
        src_cpp/runtime/app_paths.h
        src_cpp/runtime/character_asset_catalog.cpp
        src_cpp/runtime/character_asset_catalog.h
        src_cpp/ui/entity_positions.cpp
        src_cpp/ui/entity_positions.h
        src_cpp/ui/entity_widget.cpp
        src_cpp/ui/entity_widget.h
    )

    target_include_directories(CyberCompanionCppWidgetTests PRIVATE src_cpp)

    target_link_libraries(CyberCompanionCppWidgetTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
            Qt6::Test
    )

    qt_add_executable(CyberCompanionCppTrayTests
        tests_cpp/test_tray_controller.cpp
        src_cpp/ui/tray_controller.cpp
        src_cpp/ui/tray_controller.h
        src_cpp/runtime/app_paths.cpp
        src_cpp/runtime/app_paths.h
        src_cpp/runtime/character_manifest_catalog.cpp
        src_cpp/runtime/character_manifest_catalog.h
    )

    target_include_directories(CyberCompanionCppTrayTests PRIVATE src_cpp)

    target_link_libraries(CyberCompanionCppTrayTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
            Qt6::Test
    )

    qt_add_executable(CyberCompanionCppSettingsTests
        tests_cpp/test_settings_dialog.cpp
        src_cpp/services/openai_compatible_client.cpp
        src_cpp/services/openai_compatible_client.h
        src_cpp/ui/settings_dialog.cpp
        src_cpp/ui/settings_dialog.h
        src_cpp/runtime/app_config.h
    )

    target_include_directories(CyberCompanionCppSettingsTests PRIVATE src_cpp)

    target_link_libraries(CyberCompanionCppSettingsTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Network
            Qt6::Widgets
            Qt6::Test
    )

    qt_add_executable(CyberCompanionCppTrajectoryTests
        tests_cpp/test_trajectory_player.cpp
        src_cpp/ui/trajectory_player.cpp
        src_cpp/ui/trajectory_player.h
    )

    target_include_directories(CyberCompanionCppTrajectoryTests PRIVATE src_cpp)

    target_link_libraries(CyberCompanionCppTrajectoryTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
            Qt6::Test
    )

    qt_add_executable(CyberCompanionCppDirectorTests
        tests_cpp/test_runtime_director.cpp
        src_cpp/runtime/app_paths.cpp
        src_cpp/runtime/app_paths.h
        src_cpp/runtime/scripted_trajectory_catalog.cpp
        src_cpp/runtime/scripted_trajectory_catalog.h
        src_cpp/runtime/presence_detector.cpp
        src_cpp/runtime/presence_detector.h
        src_cpp/runtime/voice_script_catalog.cpp
        src_cpp/runtime/voice_script_catalog.h
        src_cpp/runtime/runtime_director.cpp
        src_cpp/runtime/runtime_director.h
    )

    target_include_directories(CyberCompanionCppDirectorTests PRIVATE src_cpp)

    target_link_libraries(CyberCompanionCppDirectorTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Test
    )

    qt_add_executable(CyberCompanionCppCommentaryTests
        tests_cpp/test_openai_commentary_service.cpp
        src_cpp/services/openai_compatible_client.cpp
        src_cpp/services/openai_compatible_client.h
        src_cpp/services/openai_commentary_service.cpp
        src_cpp/services/openai_commentary_service.h
        src_cpp/runtime/screen_capture.cpp
        src_cpp/runtime/screen_capture.h
        src_cpp/services/service_contracts.h
    )

    target_include_directories(CyberCompanionCppCommentaryTests PRIVATE src_cpp)

    target_link_libraries(CyberCompanionCppCommentaryTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Network
            Qt6::Test
    )

    qt_add_executable(CyberCompanionCppAudioTests
        tests_cpp/test_audio_service.cpp
        src_cpp/services/audio_service.cpp
        src_cpp/services/audio_service.h
        src_cpp/services/service_contracts.h
    )

    target_include_directories(CyberCompanionCppAudioTests PRIVATE src_cpp)

    target_link_libraries(CyberCompanionCppAudioTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Multimedia
            Qt6::Test
    )

    qt_add_executable(CyberCompanionCppVoiceInputTests
        tests_cpp/test_voice_input_service.cpp
        src_cpp/runtime/voice_command_matcher.cpp
        src_cpp/runtime/voice_command_matcher.h
        src_cpp/services/voice_input_service.cpp
        src_cpp/services/voice_input_service.h
        src_cpp/services/openai_compatible_client.cpp
        src_cpp/services/openai_compatible_client.h
        src_cpp/services/service_contracts.h
    )

    target_include_directories(CyberCompanionCppVoiceInputTests PRIVATE src_cpp)

    target_link_libraries(CyberCompanionCppVoiceInputTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Network
            Qt6::Multimedia
            Qt6::Test
    )

    qt_add_executable(CyberCompanionCppAudioMonitorTests
        tests_cpp/test_audio_output_monitor_service.cpp
        src_cpp/services/audio_output_monitor_service.cpp
        src_cpp/services/audio_output_monitor_service.h
        src_cpp/services/service_contracts.h
    )

    target_include_directories(CyberCompanionCppAudioMonitorTests PRIVATE src_cpp)

    target_link_libraries(CyberCompanionCppAudioMonitorTests
        PRIVATE
            Qt6::Core
            Qt6::Test
    )

    if(WIN32)
        target_link_libraries(CyberCompanionCppAudioMonitorTests PRIVATE Ole32 Uuid)
    endif()

    qt_add_executable(CyberCompanionCppIdleInvasionTests
        tests_cpp/test_idle_invasion_controller.cpp
        src_cpp/runtime/idle_invasion_controller.cpp
        src_cpp/runtime/idle_invasion_controller.h
        src_cpp/runtime/app_paths.cpp
        src_cpp/runtime/app_paths.h
    )

    target_include_directories(CyberCompanionCppIdleInvasionTests PRIVATE src_cpp)

    target_link_libraries(CyberCompanionCppIdleInvasionTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
            Qt6::Test
    )

    add_test(NAME CyberCompanionCppConfigTests COMMAND CyberCompanionCppConfigTests)
    add_test(NAME CyberCompanionCppAssetTests COMMAND CyberCompanionCppAssetTests)
    add_test(NAME CyberCompanionCppAutoStartTests COMMAND CyberCompanionCppAutoStartTests)
    add_test(NAME CyberCompanionCppCharacterManifestTests COMMAND CyberCompanionCppCharacterManifestTests)
    add_test(NAME CyberCompanionCppCharacterSwitchTests COMMAND CyberCompanionCppCharacterSwitchTests)
    add_test(NAME CyberCompanionCppVoiceScriptTests COMMAND CyberCompanionCppVoiceScriptTests)
    add_test(NAME CyberCompanionCppVoiceCommandTests COMMAND CyberCompanionCppVoiceCommandTests)
    add_test(NAME CyberCompanionCppVersionTests COMMAND CyberCompanionCppVersionTests)
    add_test(NAME CyberCompanionCppPresenceTests COMMAND CyberCompanionCppPresenceTests)
    add_test(NAME CyberCompanionCppVisionAnalyzerTests COMMAND CyberCompanionCppVisionAnalyzerTests)
    add_test(NAME CyberCompanionCppTrajectoryCatalogTests COMMAND CyberCompanionCppTrajectoryCatalogTests)
    add_test(NAME CyberCompanionCppUiTests COMMAND CyberCompanionCppUiTests)
    add_test(NAME CyberCompanionCppWidgetTests COMMAND CyberCompanionCppWidgetTests)
    add_test(NAME CyberCompanionCppTrayTests COMMAND CyberCompanionCppTrayTests)
    add_test(NAME CyberCompanionCppSettingsTests COMMAND CyberCompanionCppSettingsTests)
    add_test(NAME CyberCompanionCppTrajectoryTests COMMAND CyberCompanionCppTrajectoryTests)
    add_test(NAME CyberCompanionCppDirectorTests COMMAND CyberCompanionCppDirectorTests)
    add_test(NAME CyberCompanionCppCommentaryTests COMMAND CyberCompanionCppCommentaryTests)
    add_test(NAME CyberCompanionCppAudioTests COMMAND CyberCompanionCppAudioTests)
    add_test(NAME CyberCompanionCppVoiceInputTests COMMAND CyberCompanionCppVoiceInputTests)
    add_test(NAME CyberCompanionCppAudioMonitorTests COMMAND CyberCompanionCppAudioMonitorTests)
    add_test(NAME CyberCompanionCppIdleInvasionTests COMMAND CyberCompanionCppIdleInvasionTests)
endif()
