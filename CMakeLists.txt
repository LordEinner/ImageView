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
    src/platform.cpp src/platform.h
)
qt_add_resources(imageview "resources" PREFIX "/" BASE src FILES src/icon.svg)

target_compile_features(imageview PRIVATE cxx_std_20)
target_compile_definitions(imageview PRIVATE QT_NO_KEYWORDS)
target_link_libraries(imageview PRIVATE Qt6::Widgets Qt6::Svg)

if(MSVC)
    # Sources are UTF-8 (×, — in UI strings).
    target_compile_options(imageview PRIVATE /W4 /utf-8 /permissive-)
else()
    target_compile_options(imageview PRIVATE -Wall -Wextra)
endif()

if(WIN32)
    # GUI app (no console window), with an icon and version info for Explorer.
    set_target_properties(imageview PROPERTIES WIN32_EXECUTABLE TRUE OUTPUT_NAME ImageView)

    # The .ico is rendered from src/icon.svg by a small helper built first.
    qt_add_executable(make_ico packaging/windows/make_ico.cpp)
    set_target_properties(make_ico PROPERTIES WIN32_EXECUTABLE FALSE)
    target_link_libraries(make_ico PRIVATE Qt6::Gui Qt6::Svg)
    get_target_property(qt_core_dll Qt6::Core IMPORTED_LOCATION_RELEASE)
    if(NOT qt_core_dll)
        get_target_property(qt_core_dll Qt6::Core LOCATION)
    endif()
    get_filename_component(qt_bin_dir "${qt_core_dll}" DIRECTORY)
    set(app_ico "${CMAKE_CURRENT_BINARY_DIR}/imageview.ico")
    add_custom_command(
        OUTPUT "${app_ico}"
        COMMAND ${CMAKE_COMMAND} -E env "--modify" "PATH=path_list_prepend:${qt_bin_dir}"
                $<TARGET_FILE:make_ico> "${CMAKE_CURRENT_SOURCE_DIR}/src/icon.svg" "${app_ico}"
        DEPENDS make_ico src/icon.svg
        COMMENT "Rendering imageview.ico"
        VERBATIM
    )
    target_sources(imageview PRIVATE packaging/windows/imageview.rc "${app_ico}" packaging/windows/imageview.manifest)
    target_link_libraries(imageview PRIVATE comctl32 shell32)
    set_source_files_properties(packaging/windows/imageview.rc PROPERTIES OBJECT_DEPENDS "${app_ico}")
    target_include_directories(imageview PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

    # `cmake --install build --prefix dist/ImageView` gives a self-contained
    # folder: the exe, the Qt DLLs and plugins (windeployqt) and the MSVC runtime.
    # The DLLs go flat next to the exe, where Windows looks for them.
    install(TARGETS imageview RUNTIME DESTINATION .)
    qt_generate_deploy_script(
        TARGET imageview
        OUTPUT_SCRIPT deploy_script
        CONTENT "
qt_deploy_runtime_dependencies(
    EXECUTABLE \"$<TARGET_FILE_NAME:imageview>\"
    BIN_DIR .
    PLUGINS_DIR plugins
    NO_TRANSLATIONS
    NO_COMPILER_RUNTIME
    DEPLOY_TOOL_OPTIONS --no-system-d3d-compiler --no-system-dxc-compiler --no-opengl-sw
                        --no-network --skip-plugin-types tls,networkinformation,generic
)")
    install(SCRIPT ${deploy_script})
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
    set(CMAKE_INSTALL_UCRT_LIBRARIES FALSE)
    include(InstallRequiredSystemLibraries)
else()
    include(GNUInstallDirs)
    install(TARGETS imageview RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES packaging/imageview.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES src/icon.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps RENAME imageview.svg)
endif()
