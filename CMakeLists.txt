cmake_minimum_required(VERSION 3.19)
project(Zero-Time-Lost-tobedone LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets OpenGLWidgets Concurrent Sql)

qt_standard_project_setup()

qt_add_executable(Zero-Time-Lost-tobedone
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui

    manager/configmanager.h manager/configmanager.cpp
    manager/cachemanager.h manager/cachemanager.cpp
    manager/mailaccountmanager.h manager/mailaccountmanager.cpp
    manager/mailmanager.h manager/mailmanager.cpp

    help/constants.h
    help/datatype.h
    help/helpjsonioengine.h help/helpjsonioengine.cpp
    help/helpsqliteioengine.h help/helpsqliteioengine.cpp
    help/logger.h

    home/homebackgroundshader.h home/homebackgroundshader.cpp
    home/homeinfowidget.h home/homeinfowidget.cpp
    home/homewidget.h home/homewidget.cpp
    home/homeweatherservice.h home/homeweatherservice.cpp
    home/homequoteservice.h home/homequoteservice.cpp

    mail/mailwidget.h mail/mailwidget.cpp
    mail/mailgetservice.h mail/mailgetservice.cpp
    mail/mailsimpledisplaywidget.h mail/mailsimpledisplaywidget.cpp
    mail/mailsimplecard.h mail/mailsimplecard.cpp
    mail/mailbodyreader.h mail/mailbodyreader.cpp

    setting/settingwidget.h setting/settingwidget.cpp
    setting/settinghomepage.h setting/settinghomepage.cpp
    setting/settingmailpage.h setting/settingmailpage.cpp
    setting/settingmailaccountswidget.h setting/settingmailaccountswidget.cpp
    setting/settingdeadlinepage.h setting/settingdeadlinepage.cpp

    deadline/deadlinewidget.h deadline/deadlinewidget.cpp






)


target_include_directories(Zero-Time-Lost-tobedone PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/help
    ${CMAKE_CURRENT_SOURCE_DIR}/manager
    ${CMAKE_CURRENT_SOURCE_DIR}/home
    ${CMAKE_CURRENT_SOURCE_DIR}/setting
    ${CMAKE_CURRENT_SOURCE_DIR}/mail
    ${CMAKE_CURRENT_SOURCE_DIR}/deadline
)

target_link_libraries(Zero-Time-Lost-tobedone
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::OpenGLWidgets
        Qt::Concurrent
        Qt::Sql
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
