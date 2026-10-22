cmake_minimum_required(VERSION 3.22)

project(ZenithBrowser VERSION 1.0.0 LANGUAGES CXX)
set(ZENITH_VERSION_SUFFIX "" CACHE STRING "Release suffix; empty for a stable release")
set(ZENITH_RELEASE_VERSION "${PROJECT_VERSION}${ZENITH_VERSION_SUFFIX}")
file(WRITE "${CMAKE_BINARY_DIR}/zenith-version.txt" "${ZENITH_RELEASE_VERSION}\n")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS
    Widgets
    Svg
    Network
    Concurrent
    DBus
    Sql
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(WEBKITGTK REQUIRED IMPORTED_TARGET webkit2gtk-4.1>=2.40)
find_package(X11 REQUIRED)

qt_standard_project_setup()

option(ZENITH_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
if(ZENITH_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

qt_add_executable(zenith-browser
    src/core/Preferences.cpp
    src/core/GtkClipboardGuard.cpp
    src/core/Navigation.cpp
    src/core/SessionStore.cpp
    src/core/BrowserData.cpp
    src/core/BookmarkCodec.cpp
    src/core/DownloadManager.h
    src/core/DownloadManager.cpp
    src/main.cpp
    src/ui/ThemeManager.cpp
    src/ui/BrowserMenu.cpp
    src/ui/InternalPageStack.cpp
    src/ui/FindBar.cpp
    src/ui/StartPage.cpp
    src/ui/PageWidgets.cpp
    src/ui/ThemeManager.h
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
    src/core/WebProfile.h
    src/core/WebProfile.cpp
    src/core/WebView.h
    src/core/WebView.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/MainWindowTools.cpp
    src/ui/LibraryPage.cpp
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
target_compile_definitions(zenith-browser PRIVATE ZENITH_VERSION="${ZENITH_RELEASE_VERSION}")

target_link_libraries(zenith-browser PRIVATE
    Qt6::Widgets
    Qt6::Svg
    Qt6::Network
    Qt6::Concurrent
    Qt6::DBus
    Qt6::Sql
    PkgConfig::WEBKITGTK
    X11::X11
    ${CMAKE_DL_LIBS}
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(zenith-browser PRIVATE "LINKER:--export-dynamic-symbol=gtk_clipboard_request_targets")
endif()

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
    pkg_check_modules(LIBSECRET REQUIRED IMPORTED_TARGET libsecret-1)
    target_link_libraries(zenith-browser PRIVATE PkgConfig::LIBSECRET)
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(zenith-core-tests
        src/core/Preferences.cpp
    src/core/GtkClipboardGuard.cpp
        src/ui/MainWindow.cpp
        src/ui/MainWindowTools.cpp
        src/ui/LibraryPage.cpp
        src/ui/SplitView.cpp
        src/ui/ThemeManager.cpp
        src/ui/BrowserMenu.cpp
        src/ui/InternalPageStack.cpp
    src/ui/FindBar.cpp
        src/ui/StartPage.cpp
        src/ui/PageWidgets.cpp
        src/ui/SettingsPage.cpp
        src/ui/Sidebar.cpp
        src/ui/Sidebar.h
        src/ui/TabItemWidget.cpp
        src/ui/TabItemWidget.h
        src/ui/UrlBar.cpp
        src/ui/UrlBar.h
        src/ui/ZenithIcons.cpp
        resources/resources.qrc
        src/core/Navigation.cpp
        src/core/SessionStore.cpp
        src/core/BrowserData.cpp
        src/core/BookmarkCodec.cpp
        src/core/DownloadManager.h
        src/core/DownloadManager.cpp
        tests/CoreTests.cpp
        src/core/TabManager.h
        src/core/TabManager.cpp
        src/core/AdBlockInterceptor.h
        src/core/AdBlockInterceptor.cpp
        src/core/AdBlockRuleParser.h
        src/core/AdBlockRuleParser.cpp
        src/core/AdBlockUpdater.h
        src/core/AdBlockUpdater.cpp
        src/core/CredentialStore.h
        src/core/CredentialStore.cpp
        src/core/CredentialManager.h
        src/core/CredentialManager.cpp
        src/core/WebProfile.h
        src/core/WebProfile.cpp
        src/core/WebView.h
        src/core/WebView.cpp
    )
    target_include_directories(zenith-core-tests PRIVATE src)
    target_compile_definitions(zenith-core-tests PRIVATE ZENITH_VERSION="${ZENITH_RELEASE_VERSION}")
    target_link_libraries(zenith-core-tests PRIVATE
        Qt6::Test
        Qt6::Network
        Qt6::Concurrent
        Qt6::Widgets
        Qt6::DBus
        Qt6::Sql
        Qt6::Svg
        PkgConfig::WEBKITGTK
        X11::X11
        ${CMAKE_DL_LIBS}
    )
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_options(zenith-core-tests PRIVATE "LINKER:--export-dynamic-symbol=gtk_clipboard_request_targets")
    endif()
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
install(FILES packaging/zenith-browser.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES resources/icons/zenith.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps RENAME zenith-browser.svg)
install(FILES resources/icons/zenith.png DESTINATION ${CMAKE_INSTALL_DATADIR}/pixmaps RENAME zenith-browser.png)
install(FILES README.md DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/zenith-browser)
install(FILES docs/VALIDATION.md docs/ROADMAP.md docs/OUTLOOK-COMPATIBILITY.md docs/MEMORY.md docs/ADBLOCK.md docs/SECURITY.md docs/APPEARANCE.md docs/RELEASE.md docs/PLANO-1.0.md docs/PREFERENCES.md docs/EXECUCAO-1.0.md DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/zenith-browser/docs)
set(CPACK_PACKAGE_VERSION ${ZENITH_RELEASE_VERSION})
set(CPACK_PACKAGE_CONTACT "Zenith Browser")
set(CPACK_GENERATOR TGZ)
include(CPack)
