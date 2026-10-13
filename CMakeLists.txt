cmake_minimum_required(VERSION 3.21)
project(IronwatchConsole VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Network)
find_package(Qt6 QUIET COMPONENTS WebEngineQuick)

if(Qt6WebEngineQuick_FOUND)
    target_link_libraries(IronwatchConsole PRIVATE Qt6::WebEngineQuick)
    target_compile_definitions(IronwatchConsole PRIVATE IRONWATCH_HAS_WEBENGINE)
    # Only add the WebEngine-importing QML file when the module exists, so
    # qmlcachegen/qmllint never see `import QtWebEngine` without it.
    qt_target_qml_sources(IronwatchConsole QML_FILES qml/windows/ReferencePaneWeb.qml)
    message(STATUS "Threat Intel: Qt WebEngine found — embedded reference pane enabled")
else()
    message(STATUS "Threat Intel: Qt WebEngine NOT found — using system-browser fallback")
endif()

# Qt MQTT (QtMqtt). This is an add-on module, NOT part of the default Qt
# installation — tick "Qt Mqtt" in the Qt Maintenance Tool / online
# installer, or build it from https://github.com/qt/qtmqtt against your Qt
# version. Building from source additionally needs Qt's private headers
# (qt6-base-private-dev on Debian/Ubuntu).
#
# Licensing note: QtMqtt ships under GPLv3 or a commercial Qt licence — it
# is not LGPL like Qt Core. That is fine for an internal or open-source
# tool; a closed-source binary needs the commercial licence.
find_package(Qt6 6.4 REQUIRED COMPONENTS Mqtt)

qt_standard_project_setup(REQUIRES 6.4)

qt_add_executable(IronwatchConsole
    src/main.cpp
    src/AppController.h
    src/AppController.cpp
    src/ThemeProvider.h
    src/ThemeProvider.cpp
    src/core/DetectionEvent.h
    src/core/DetectionEvent.cpp
    src/core/Node.h
    src/core/Node.cpp
    src/core/NodesModel.h
    src/core/NodesModel.cpp
    src/core/Detection.h
    src/core/Detection.cpp
    src/core/DetectionsModel.h
    src/core/DetectionsModel.cpp
    src/core/EventStreamModel.h
    src/core/EventStreamModel.cpp
    src/core/LogModel.h
    src/core/LogModel.cpp
    src/core/ClassStatsModel.h
    src/core/ClassStatsModel.cpp
    src/core/ChatModel.h
    src/core/ChatModel.cpp
    src/core/EventFilters.h
    src/core/EventFilters.cpp
    src/core/SiemEventsModel.h
    src/core/SiemEventsModel.cpp
    src/services/BackendSettings.h
    src/services/BackendSettings.cpp
    src/services/ChatClient.h
    src/services/ChatClient.cpp
    src/services/AlertExplainer.h
    src/services/AlertExplainer.cpp
    src/services/EventWebLink.h
    src/services/EventWebLink.cpp
    src/services/SiemClient.h
    src/services/SiemClient.cpp
    src/services/MqttClient.h
    src/services/MqttClient.cpp
    src/services/TelemetryClient.h
    src/services/TelemetryClient.cpp
    src/services/threat_intel/IntelClient.h
    src/services/threat_intel/IntelClient.cpp
    src/services/ThreatIntelController.cpp
    src/services/ThreatIntelController.h
    src/services/threat_intel/NewsModel.cpp
    src/services/threat_intel/NewsModel.h
    src/services/threat_intel/ThreatGraphModel.cpp
    src/services/threat_intel/ThreatGraphModel.h
)

qt_add_qml_module(IronwatchConsole
    URI Ironwatch
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/util/Series.js
        qml/util/Citations.js
        qml/components/BlueprintFrame.qml
        qml/components/AppButton.qml
        qml/components/TagBadge.qml
        qml/components/SegmentedControl.qml
        qml/components/StatCard.qml
        qml/components/FormField.qml
        qml/components/RadioOption.qml
        qml/components/LineChart.qml
        qml/components/SectionLabel.qml
        qml/components/SidebarNavItem.qml
        qml/components/TitleBarButton.qml
        qml/components/NavIcon.qml
        qml/components/EmptyState.qml
        qml/components/AlertExplainPopup.qml
        qml/screens/OverviewScreen.qml
        qml/screens/DetectionsScreen.qml
        qml/screens/NodesScreen.qml
        qml/screens/ModelScreen.qml
        qml/screens/IntegrationsScreen.qml
        qml/screens/ChatScreen.qml
        qml/screens/EventsScreen.qml
        qml/windows/ThreatIntelWindow.qml
    RESOURCES
        assets/fonts/README.md
        SOURCES src/core/ChatModel.cpp src/core/ChatModel.h src/core/ClassStatsModel.cpp src/core/ClassStatsModel.h src/core/Detection.cpp src/core/Detection.h src/core/DetectionEvent.cpp src/core/DetectionEvent.h src/core/DetectionsModel.cpp src/core/DetectionsModel.h src/core/EventFilters.cpp src/core/EventFilters.h src/core/EventStreamModel.cpp src/core/EventStreamModel.h src/core/LogModel.cpp src/core/LogModel.h src/core/Node.cpp src/core/Node.h src/core/NodesModel.cpp src/core/NodesModel.h src/core/SiemEventsModel.cpp src/core/SiemEventsModel.h
)

target_link_libraries(IronwatchConsole PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
    Qt6::Mqtt
)

target_include_directories(IronwatchConsole PRIVATE src)

set_target_properties(IronwatchConsole PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE FALSE
)

# Console-free probe for the backend. Links the real ChatClient and
# SiemClient, so it answers "is it the backend or the console?" for both APIs
# without starting a GUI. See app/test/chat_probe.cpp.
qt_add_executable(iw-chat-probe
    test/chat_probe.cpp
    src/core/ChatModel.h
    src/core/ChatModel.cpp
    src/core/DetectionEvent.h
    src/core/DetectionEvent.cpp
    src/core/EventFilters.h
    src/core/EventFilters.cpp
    src/core/SiemEventsModel.h
    src/core/SiemEventsModel.cpp
    src/services/BackendSettings.h
    src/services/BackendSettings.cpp
    src/services/ChatClient.h
    src/services/ChatClient.cpp
    src/services/SiemClient.h
    src/services/SiemClient.cpp
    src/services/ThreatIntelController.h
    src/services/ThreatIntelController.cpp
    src/services/threat_intel/IntelClient.h
    src/services/threat_intel/IntelClient.cpp
    src/services/threat_intel/ThreatGraphModel.h
    src/services/threat_intel/ThreatGraphModel.cpp
    src/services/threat_intel/NewsModel.h
    src/services/threat_intel/NewsModel.cpp

)
target_link_libraries(iw-chat-probe PRIVATE Qt6::Core Qt6::Network)
target_include_directories(iw-chat-probe PRIVATE src)
set_target_properties(iw-chat-probe PROPERTIES WIN32_EXECUTABLE FALSE)

# Qt deployment on Windows: run windeployqt on the built .exe to pull in the
# Qt runtime, the QML plugins and Qt6Mqtt.dll.
#   windeployqt --qmldir app/qml build/Release/IronwatchConsole.exe

# The previous TCP test harness (tools/fake_board.cpp) is superseded by
# board/test/fake_board_mqtt.cpp, which publishes through the broker using
# the real board-side publisher.
