cmake_minimum_required(VERSION 3.20)
project(SylvaniaLauncher VERSION 2.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS 
    Core 
    Widgets 
    Network 
    Multimedia
)

# Source files
set(SOURCES
    src/main.cpp
    src/core/ConfigManager.cpp
    src/core/SoundManager.cpp
    src/core/NotesManager.cpp
    src/ui/WowButton.cpp
    src/ui/MainWindow.cpp
    src/ui/NotesWindow.cpp
    src/ui/RealmlistWindow.cpp
    src/ui/DownloadDialog.cpp
    src/ui/SettingsDialog.cpp
    src/ui/ServerEditDialog.cpp
    src/utils/PathUtils.cpp
    src/core/HdPatchManager.cpp
)

# Header files
set(HEADERS
    src/core/ConfigManager.h
    src/core/SoundManager.h
    src/core/NotesManager.h
    src/ui/WowButton.h
    src/ui/MainWindow.h
    src/ui/NotesWindow.h
    src/ui/RealmlistWindow.h
    src/ui/DownloadDialog.h
    src/ui/SettingsDialog.h
    src/ui/ServerEditDialog.h
    src/utils/PathUtils.h
    src/core/HdPatchManager.h
)

# Resources
set(RESOURCES
    resources/resources.qrc
)

# Create executable
add_executable(${PROJECT_NAME} WIN32
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Multimedia
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
)

# Windows specific settings
if(WIN32)
    # Set application icon - DISABLED due to build errors with spaces in paths
    # set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc")
    # target_sources(${PROJECT_NAME} PRIVATE ${APP_ICON_RESOURCE_WINDOWS})
    
    # Copy Qt DLLs to output directory
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:Qt6::Core>
        $<TARGET_FILE:Qt6::Widgets>
        $<TARGET_FILE:Qt6::Network>
        $<TARGET_FILE:Qt6::Multimedia>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
endif()

# Installation
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# Copy assets to build directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/../Asset"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Asset"
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/../Sounds"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Sounds"
)
