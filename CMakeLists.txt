cmake_minimum_required(VERSION 3.16)

project(ShearPlate VERSION 0.1.11 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt 会自动处理 moc / rcc / uic
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

# WIN32: 不弹出控制台窗口的 GUI 程序
set(ShearPlate_SOURCES
    src/main.cpp
    src/ClipboardApp.cpp
    src/ClipboardApp.h
    src/ClipboardStore.cpp
    src/ClipboardStore.h
    src/ClipboardManager.cpp
    src/ClipboardManager.h
    src/ClipboardItem.h
    src/ClipboardListModel.cpp
    src/ClipboardListModel.h
    src/ClipboardItemDelegate.cpp
    src/ClipboardItemDelegate.h
    src/ClipboardListView.cpp
    src/ClipboardListView.h
    src/HoverIconButton.cpp
    src/HoverIconButton.h
    src/ClipboardPanel.cpp
    src/ClipboardPanel.h
    src/TrayController.cpp
    src/TrayController.h
    src/SettingsDialog.cpp
    src/SettingsDialog.h
    src/AppSettings.h
    src/Theme.h
    src/Util.h
    src/WinUtil.cpp
    src/WinUtil.h
    src/GlobalHotkey.cpp
    src/GlobalHotkey.h
    resources.qrc
)

# Windows 可执行文件图标：把 favicon.ico 注入 exe
if(WIN32)
    set(ICON_RESOURCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/images/favicon.ico")
    set(VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
    set(VERSION_MINOR "${PROJECT_VERSION_MINOR}")
    set(VERSION_PATCH "${PROJECT_VERSION_PATCH}")
    set(VERSION "${PROJECT_VERSION}")
    configure_file(app.rc.in ${CMAKE_CURRENT_BINARY_DIR}/app.rc @ONLY)
    list(APPEND ShearPlate_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/app.rc)
endif()

add_executable(ShearPlate WIN32 ${ShearPlate_SOURCES})

target_link_libraries(ShearPlate PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

# Windows 上 SendInput / 前台窗口等系统 API
if(WIN32)
    target_link_libraries(ShearPlate PRIVATE user32)
endif()

# 安装规则（可选）
install(TARGETS ShearPlate
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
