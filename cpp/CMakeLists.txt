cmake_minimum_required(VERSION 3.20)
project(SylvaniaLauncher VERSION 2.7 LANGUAGES CXX)

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
    Concurrent
    LinguistTools
)

# Linux: find libzip for native ZIP extraction
if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBZIP REQUIRED libzip)
endif()

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
    src/ui/AddonsWindow.cpp
    src/utils/PathUtils.cpp
    src/utils/ZipExtractor.cpp
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
    src/ui/AddonsWindow.h
    src/utils/PathUtils.h
    src/utils/ZipExtractor.h
    src/core/HdPatchManager.h
)

# Linux-only sources: WineManager
if(UNIX AND NOT APPLE)
    list(APPEND SOURCES src/core/WineManager.cpp)
    list(APPEND HEADERS src/core/WineManager.h)
endif()

# Resources
set(RESOURCES
    resources/resources.qrc
)

# Translations
set(TS_FILES
    resources/translations/SylvaniaLauncher_en.ts
)
set_source_files_properties(${TS_FILES} PROPERTIES OUTPUT_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/../Asset/translations)
qt_add_translation(QM_FILES ${TS_FILES})

# Create executable
# WIN32 flag is only applied on Windows (hides console window)
if(WIN32)
    add_executable(${PROJECT_NAME} WIN32
        ${SOURCES}
        ${HEADERS}
        ${RESOURCES}
        ${TS_FILES}
        ${QM_FILES}
    )
else()
    add_executable(${PROJECT_NAME}
        ${SOURCES}
        ${HEADERS}
        ${RESOURCES}
        ${TS_FILES}
        ${QM_FILES}
    )
endif()

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Multimedia
    Qt6::Concurrent
)

# Linux: link libzip
if(UNIX AND NOT APPLE)
    target_include_directories(${PROJECT_NAME} PRIVATE ${LIBZIP_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBZIP_LIBRARIES})
endif()

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
)

# Windows specific settings
if(WIN32)
    set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc")
    set(APP_ICON_OBJ "${CMAKE_CURRENT_BINARY_DIR}/app_icon.obj")
    
    add_custom_command(OUTPUT "${APP_ICON_OBJ}"
        COMMAND windres -J rc -O coff app.rc -o "${APP_ICON_OBJ}"
        DEPENDS "${APP_ICON_RESOURCE_WINDOWS}" "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.ico"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/resources"
        VERBATIM
    )
    
    set_source_files_properties("${APP_ICON_OBJ}" PROPERTIES EXTERNAL_OBJECT true GENERATED true)
    target_sources(${PROJECT_NAME} PRIVATE "${APP_ICON_OBJ}")
    
    # Copy Qt DLLs to output directory
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:Qt6::Core>
        $<TARGET_FILE:Qt6::Widgets>
        $<TARGET_FILE:Qt6::Network>
        $<TARGET_FILE:Qt6::Multimedia>
        $<TARGET_FILE:Qt6::Concurrent>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
endif()

# Installation
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# Linux: Install .desktop file and icon
if(UNIX AND NOT APPLE)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/../linux/sylvania-launcher.desktop"
            DESTINATION share/applications)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/../Asset/LogoSylvania/LogoSylvania256.png"
            DESTINATION share/icons/hicolor/256x256/apps
            RENAME sylvania-launcher.png)
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../Asset"
            DESTINATION share/sylvania-launcher)
endif()

# Copy assets to build directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/../Asset"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Asset"
)
