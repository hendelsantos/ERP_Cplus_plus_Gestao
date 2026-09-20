cmake_minimum_required(VERSION 3.21)

project(MHStore VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Quick Sql)

qt_standard_project_setup(REQUIRES 6.4)

qt_add_executable(MHStore
    src/infrastructure/backup/backup.cpp
    src/infrastructure/backup/backup.h
    src/modules/pos/pos.cpp
    src/modules/pos/pos.h
    src/modules/inventory/inventory.cpp
    src/modules/inventory/inventory.h
    src/modules/catalog/catalog.cpp
    src/modules/catalog/catalog.h
    src/main.cpp
    src/core/database/database.cpp
    src/core/database/database.h
)

set_target_properties(MHStore PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

qt_add_qml_module(MHStore
    URI MHStore
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/CatalogPage.qml
        qml/InventoryPage.qml
        qml/PosPage.qml
        qml/SalesPage.qml
        qml/BackupPage.qml
        qml/SettingsPage.qml
)

target_link_libraries(MHStore PRIVATE
    Qt6::Quick
    Qt6::Sql
)

include(GNUInstallDirs)
install(TARGETS MHStore
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Test)
    qt_add_executable(catalog_tests
        tests/catalog_test.cpp
        src/modules/catalog/catalog.cpp
        src/modules/catalog/catalog.h
        src/core/database/database.cpp
        src/core/database/database.h
    )
    target_include_directories(catalog_tests PRIVATE src)
    target_link_libraries(catalog_tests PRIVATE Qt6::Test Qt6::Sql)
    add_test(NAME catalog_tests COMMAND catalog_tests)
    qt_add_executable(inventory_tests
        tests/inventory_test.cpp
        src/modules/inventory/inventory.cpp
        src/modules/inventory/inventory.h
        src/core/database/database.cpp
        src/core/database/database.h
    )
    target_include_directories(inventory_tests PRIVATE src)
    target_link_libraries(inventory_tests PRIVATE Qt6::Test Qt6::Sql)
    add_test(NAME inventory_tests COMMAND inventory_tests)
    qt_add_executable(pos_tests
        tests/pos_test.cpp
        src/modules/pos/pos.cpp
        src/modules/pos/pos.h
        src/core/database/database.cpp
        src/core/database/database.h
    )
    target_include_directories(pos_tests PRIVATE src)
    target_link_libraries(pos_tests PRIVATE Qt6::Test Qt6::Sql)
    add_test(NAME pos_tests COMMAND pos_tests)
    qt_add_executable(backup_tests
        tests/backup_test.cpp
        src/infrastructure/backup/backup.cpp
        src/infrastructure/backup/backup.h
        src/core/database/database.cpp
        src/core/database/database.h
    )
    target_include_directories(backup_tests PRIVATE src)
    target_link_libraries(backup_tests PRIVATE Qt6::Test Qt6::Sql)
    add_test(NAME backup_tests COMMAND backup_tests)
    qt_add_executable(ui_tests
        src/infrastructure/backup/backup.cpp
        src/infrastructure/backup/backup.h
        src/modules/pos/pos.cpp
        src/modules/pos/pos.h
        tests/ui_test.cpp
        src/modules/catalog/catalog.cpp
        src/modules/catalog/catalog.h
        src/modules/inventory/inventory.cpp
        src/modules/inventory/inventory.h
        src/core/database/database.cpp
        src/core/database/database.h
    )
    target_include_directories(ui_tests PRIVATE src)
    target_compile_definitions(ui_tests PRIVATE MHSTORE_QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml")
    target_link_libraries(ui_tests PRIVATE Qt6::Quick Qt6::Test Qt6::Sql)
    add_test(NAME ui_tests COMMAND ui_tests)
    set_tests_properties(ui_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
endif()

foreach(app_target MHStore catalog_tests inventory_tests pos_tests backup_tests ui_tests)
    if(TARGET ${app_target})
        target_sources(${app_target} PRIVATE src/core/settings/settings.cpp src/core/settings/settings.h)
    endif()
endforeach()
