cmake_minimum_required(VERSION 3.19)
project(Zero-Time-Lost-tobedone LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets OpenGLWidgets)

qt_standard_project_setup()

qt_add_executable(Zero-Time-Lost-tobedone
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui

    constants.h
    homebackgroundshader.h homebackgroundshader.cpp
    homeinfowidget.h homeinfowidget.cpp
    homewidget.h homewidget.cpp
    settingwidget.h settingwidget.cpp
    configmanager.h configmanager.cpp
    datatype.h
    settinghomepage.h settinghomepage.cpp
    homeweatherservice.h homeweatherservice.cpp
    cachemanager.h cachemanager.cpp

)

target_link_libraries(Zero-Time-Lost-tobedone
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::OpenGLWidgets
)

install(TARGETS Zero-Time-Lost-tobedone
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET Zero-Time-Lost-tobedone
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
