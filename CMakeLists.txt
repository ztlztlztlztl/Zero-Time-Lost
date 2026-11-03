cmake_minimum_required(VERSION 3.19)
project(Zero-Time-Lost-tobedone VERSION 0.2.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INSTALL_BINDIR "." CACHE PATH "Z.T.L executables and runtime DLLs" FORCE)
set(CMAKE_INSTALL_LIBDIR "." CACHE PATH "Z.T.L libraries" FORCE)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets OpenGLWidgets Sql Network)
qt_standard_project_setup()
set(ZTL_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} app department repository security note home mail deadline setting manager help)
qt_add_library(ztl_components STATIC
    help/advancedfilter.cpp
    help/helpsqliteioengine.cpp
    mail/mailwidget.cpp
    mainwindow.cpp
    home/homeweatherservice.cpp
    home/homebackgroundshader.cpp
    setting/settingdeadlinepage.cpp
    setting/settinghomepage.cpp
    setting/settingmailaccountswidget.cpp
    manager/configmanager.cpp
    mail/mailbodyreader.cpp
    manager/cachemanager.cpp
    home/homeinfowidget.cpp
    setting/settingmailpage.cpp
    setting/settingwidget.cpp
    help/helpjsonioengine.cpp
    deadline/deadlinewidget.cpp
    deadline/deadlineagendawidget.cpp
    deadline/deadlineagendawidget.h
    deadline/deadlinecalendarwidget.cpp
    deadline/deadlinecalendarwidget.h
    deadline/deadlineeditor.cpp
    deadline/deadlineeditor.h
    home/homewidget.cpp
    manager/mailmanager.cpp
    manager/mailaccountmanager.cpp
    mail/mailgetservice.cpp
    mail/mailsimpledisplaywidget.cpp
    app/autostart.cpp
    app/paths.cpp
    app/businessclock.cpp
    app/processworker.cpp
    security/vaultcodec.cpp
    security/vaultcodec.h
    security/cryptotool.cpp
    repository/noterepository.cpp
    repository/noterepository.h
    help/uistyle.h
    help/accentbutton.h
    help/dialoglayout.h
    setting/settingcontrols.cpp
    setting/settingcontrols.h
    repository/taskrepository.cpp
    department/ddldepartment.cpp
    repository/vaultrepository.cpp
    department/notedepartment.cpp
    note/notecard.cpp
    note/notecard.h
    note/notestyle.h
    note/bookmarktarget.cpp
    note/bookmarktarget.h
    note/notewidget.cpp
    app/appcontext.cpp
    setting/generalsettings.cpp
    deadline/deadlinewidget.h
    mail/mailgetservice.h
    help/constants.h
    mail/mailwidget.h
    manager/mailaccountmanager.h
    setting/settingdeadlinepage.h
    home/homeweatherservice.h
    manager/mailmanager.h
    help/helpsqliteioengine.h
    help/logger.h
    manager/cachemanager.h
    help/helpjsonioengine.h
    manager/configmanager.h
    help/datatype.h
    home/homewidget.h
    setting/settingwidget.h
    mail/mailsimpledisplaywidget.h
    mainwindow.h
    setting/settinghomepage.h
    setting/settingmailaccountswidget.h
    home/homebackgroundshader.h
    home/homepalette.h
    home/homeinfowidget.h
    mail/mailbodyreader.h
    mail/mailstyle.h
    setting/settingmailpage.h
    app/paths.h
    app/businessclock.h
    app/processworker.h
    security/cryptotool.h
    repository/taskrepository.h
    department/ddldepartment.h
    repository/vaultrepository.h
    department/notedepartment.h
    note/notewidget.h
    department/homedepartment.h
    app/appcontext.h
    setting/generalsettings.h
)
target_include_directories(ztl_components PUBLIC ${ZTL_INCLUDE_DIRS})
target_link_libraries(ztl_components PUBLIC Qt6::Core Qt6::Widgets Qt6::OpenGLWidgets Qt6::Sql Qt6::Network)
if(WIN32)
    target_link_libraries(ztl_components PUBLIC bcrypt crypt32 wtsapi32 shell32 ole32)
endif()
qt_add_executable(Zero-Time-Lost-tobedone WIN32 main.cpp)
target_link_libraries(Zero-Time-Lost-tobedone PRIVATE ztl_components)
set_target_properties(Zero-Time-Lost-tobedone PROPERTIES OUTPUT_NAME "Z.T.L")
if(WIN32)
    find_program(ZTL_POWERSHELL NAMES pwsh powershell
        PATHS "$ENV{SystemRoot}/System32/WindowsPowerShell/v1.0" REQUIRED)
    # Qt Creator and direct CMake builds need the same self-contained mail runtime as the package.
    add_custom_target(ztl_mail_runtime
        COMMAND "${ZTL_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass -File
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/prepare_runtime.ps1"
            -Destination "$<TARGET_FILE_DIR:Zero-Time-Lost-tobedone>/runtime"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Zero-Time-Lost-tobedone>/scripts"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/scripts/mail_worker.py"
            "$<TARGET_FILE_DIR:Zero-Time-Lost-tobedone>/scripts/mail_worker.py"
        COMMENT "Preparing bundled mail runtime" VERBATIM)
    add_dependencies(Zero-Time-Lost-tobedone ztl_mail_runtime)
endif()
install(TARGETS Zero-Time-Lost-tobedone RUNTIME DESTINATION .)
install(FILES scripts/mail_worker.py DESTINATION scripts)
install(FILES instruct.md DESTINATION .)
install(FILES scripts/uninstall.ps1 DESTINATION .)
install(DIRECTORY "$<TARGET_FILE_DIR:Zero-Time-Lost-tobedone>/runtime/" DESTINATION runtime)
qt_generate_deploy_app_script(TARGET Zero-Time-Lost-tobedone OUTPUT_SCRIPT deploy_script NO_UNSUPPORTED_PLATFORM_ERROR)
install(SCRIPT ${deploy_script})
include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    if(WIN32)
        add_test(NAME uninstall COMMAND "${ZTL_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_uninstall.ps1")
    endif()
    qt_add_executable(ztl_phase2_tests tests/test_phase2.cpp)
    target_link_libraries(ztl_phase2_tests PRIVATE ztl_components Qt6::Test)
    add_test(NAME phase2 COMMAND ztl_phase2_tests)
    set_tests_properties(phase2 PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(ztl_phase1_tests tests/test_phase1.cpp)
    target_link_libraries(ztl_phase1_tests PRIVATE ztl_components Qt6::Test)
    add_test(NAME phase1 COMMAND ztl_phase1_tests)
    set_tests_properties(phase1 PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(ztl_vault_tests tests/test_vault.cpp)
    target_link_libraries(ztl_vault_tests PRIVATE ztl_components Qt6::Test)
    add_test(NAME vault COMMAND ztl_vault_tests)
    set_tests_properties(vault PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(ztl_note_tests tests/test_note.cpp)
    target_link_libraries(ztl_note_tests PRIVATE ztl_components Qt6::Test)
    add_test(NAME note COMMAND ztl_note_tests)
    set_tests_properties(note PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(ztl_core_tests tests/test_core.cpp)
    target_link_libraries(ztl_core_tests PRIVATE ztl_components Qt6::Test)
    add_test(NAME core COMMAND ztl_core_tests)
    set_tests_properties(core PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(ztl_ui_tests tests/test_ui.cpp)
    target_link_libraries(ztl_ui_tests PRIVATE ztl_components Qt6::Test)
    target_compile_definitions(ztl_ui_tests PRIVATE ZTL_TEST_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    if(WIN32)
        add_dependencies(ztl_core_tests ztl_mail_runtime)
        add_dependencies(ztl_ui_tests ztl_mail_runtime)
    endif()
    add_test(NAME ui COMMAND ztl_ui_tests)
    set_tests_properties(ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(ztl_mail_sync_tests tests/test_mail_sync.cpp)
    target_link_libraries(ztl_mail_sync_tests PRIVATE ztl_components Qt6::Test)
    target_compile_definitions(ztl_mail_sync_tests PRIVATE ZTL_TEST_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    if(WIN32)
        add_dependencies(ztl_mail_sync_tests ztl_mail_runtime)
    endif()
    add_test(NAME mail_sync COMMAND ztl_mail_sync_tests)
    set_tests_properties(mail_sync PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
