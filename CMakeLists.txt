cmake_minimum_required(VERSION 3.22)

project(ZenithBrowser VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Widgets
    WebEngineWidgets
    WebEngineCore
    WebChannel
    Svg
    Network
    Concurrent
)

qt_standard_project_setup()

qt_add_executable(zenith-browser
    src/main.cpp
    src/core/AdBlockInterceptor.h
    src/core/AdBlockInterceptor.cpp
    src/core/AdBlockRuleParser.h
    src/core/AdBlockRuleParser.cpp
    src/core/AdBlockUpdater.h
    src/core/AdBlockUpdater.cpp
    src/core/TabManager.h
    src/core/TabManager.cpp
    src/core/CredentialStore.h
    src/core/CredentialStore.cpp
    src/core/CredentialManager.h
    src/core/CredentialManager.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/Sidebar.h
    src/ui/Sidebar.cpp
    src/ui/TabItemWidget.h
    src/ui/TabItemWidget.cpp
    src/ui/UrlBar.h
    src/ui/UrlBar.cpp
    src/ui/SplitView.h
    src/ui/SplitView.cpp
    src/ui/SettingsPage.h
    src/ui/SettingsPage.cpp
    src/ui/ZenithIcons.h
    src/ui/ZenithIcons.cpp
    resources/resources.qrc
)

target_include_directories(zenith-browser PRIVATE src)

target_link_libraries(zenith-browser PRIVATE
    Qt6::Widgets
    Qt6::WebEngineWidgets
    Qt6::WebEngineCore
    Qt6::WebChannel
    Qt6::Svg
    Qt6::Network
    Qt6::Concurrent
)

include(CheckIPOSupported)
check_ipo_supported(RESULT ZENITH_IPO_SUPPORTED OUTPUT ZENITH_IPO_ERROR)
if(ZENITH_IPO_SUPPORTED)
    set_property(TARGET zenith-browser PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()
if(NOT MSVC)
    target_link_options(zenith-browser PRIVATE "$<$<CONFIG:Release>:-s>")
endif()

if(WIN32)
    target_link_libraries(zenith-browser PRIVATE Advapi32)
    set_target_properties(zenith-browser PROPERTIES WIN32_EXECUTABLE TRUE)
elseif(APPLE)
    target_link_libraries(zenith-browser PRIVATE "-framework Security")
    set_target_properties(zenith-browser PROPERTIES MACOSX_BUNDLE TRUE)
elseif(UNIX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBSECRET REQUIRED IMPORTED_TARGET libsecret-1)
    target_link_libraries(zenith-browser PRIVATE PkgConfig::LIBSECRET)
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(zenith-core-tests
        tests/CoreTests.cpp
        src/core/AdBlockInterceptor.h
        src/core/AdBlockInterceptor.cpp
        src/core/AdBlockRuleParser.h
        src/core/AdBlockRuleParser.cpp
        src/core/CredentialStore.h
        src/core/CredentialStore.cpp
        src/core/CredentialManager.h
        src/core/CredentialManager.cpp
    )
    target_include_directories(zenith-core-tests PRIVATE src)
    target_link_libraries(zenith-core-tests PRIVATE
        Qt6::Test
        Qt6::Widgets
        Qt6::WebEngineCore
        Qt6::WebChannel
    )
    if(WIN32)
        target_link_libraries(zenith-core-tests PRIVATE Advapi32)
    elseif(APPLE)
        target_link_libraries(zenith-core-tests PRIVATE "-framework Security")
    elseif(UNIX)
        target_link_libraries(zenith-core-tests PRIVATE PkgConfig::LIBSECRET)
    endif()
    add_test(NAME zenith-core-tests COMMAND zenith-core-tests)
endif()

include(GNUInstallDirs)
install(TARGETS zenith-browser
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
