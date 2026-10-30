cmake_minimum_required(VERSION 3.21)
project(imageview VERSION 1.0 LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt6 6.9 REQUIRED COMPONENTS Widgets Svg)
qt_standard_project_setup()

qt_add_executable(imageview
    src/main.cpp
    src/mainwindow.cpp src/mainwindow.h
    src/canvas.cpp src/canvas.h
    src/imageio.cpp src/imageio.h
    src/zoomfield.cpp src/zoomfield.h
    src/resizedialog.cpp src/resizedialog.h
    src/icons.cpp src/icons.h
)
qt_add_resources(imageview "resources" PREFIX "/" BASE src FILES src/icon.svg)

target_compile_features(imageview PRIVATE cxx_std_20)
target_compile_definitions(imageview PRIVATE QT_NO_KEYWORDS)
target_compile_options(imageview PRIVATE -Wall -Wextra)
target_link_libraries(imageview PRIVATE Qt6::Widgets Qt6::Svg)

include(GNUInstallDirs)
install(TARGETS imageview RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/imageview.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES src/icon.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps RENAME imageview.svg)
