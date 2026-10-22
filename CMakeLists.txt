cmake_minimum_required(VERSION 3.24)

project(ZenithBrowser VERSION 1.0.0 LANGUAGES CXX)
if(APPLE)
    enable_language(OBJCXX)
    set(CMAKE_OBJCXX_STANDARD 20)
    set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
    set(CMAKE_OBJCXX_EXTENSIONS OFF)
endif()
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
    Sql
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(Qt6 6.4 REQUIRED COMPONENTS DBus)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(WEBKITGTK REQUIRED IMPORTED_TARGET webkit2gtk-4.1>=2.40)
    find_package(X11 REQUIRED)
elseif(WIN32)
    set(WEBVIEW2_SDK_DIR "" CACHE PATH "Microsoft.Web.WebView2 NuGet package root")
    if(NOT WEBVIEW2_SDK_DIR)
        message(FATAL_ERROR "Set WEBVIEW2_SDK_DIR to the extracted Microsoft.Web.WebView2 NuGet package")
    endif()
    find_path(WEBVIEW2_INCLUDE_DIR WebView2.h PATHS "${WEBVIEW2_SDK_DIR}/build/native/include" NO_DEFAULT_PATH REQUIRED)
    if(CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
        set(WEBVIEW2_ARCH arm64)
    elseif(CMAKE_GENERATOR_PLATFORM STREQUAL "Win32")
        message(FATAL_ERROR "Qt 6.8 does not provide a supported Windows x86 desktop build; use x64 or ARM64")
    else()
        set(WEBVIEW2_ARCH x64)
    endif()
    find_library(WEBVIEW2_LOADER_LIBRARY WebView2LoaderStatic
        PATHS "${WEBVIEW2_SDK_DIR}/build/native/${WEBVIEW2_ARCH}" NO_DEFAULT_PATH REQUIRED)
elseif(APPLE)
    find_library(WEBKIT_FRAMEWORK WebKit REQUIRED)
    find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
    find_library(SECURITY_FRAMEWORK Security REQUIRED)
else()
    message(FATAL_ERROR "Zenith supports Linux, Windows, and macOS only")
endif()

qt_standard_project_setup()

option(ZENITH_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
if(ZENITH_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

qt_add_executable(zenith-browser
    src/core/Preferences.cpp
    src/core/Navigation.cpp
    src/core/SessionStore.cpp
    src/core/BrowserData.cpp
    src/core/BookmarkCodec.cpp
    src/core/DownloadManager.h
    src/core/DownloadManagerShared.cpp
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
    src/core/WebView.h
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

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(zenith-browser PRIVATE
        src/core/GtkClipboardGuard.cpp
        src/core/DownloadManager.cpp
        src/core/WebProfile.cpp
        src/core/WebView.cpp
    )
elseif(WIN32)
    target_sources(zenith-browser PRIVATE
        src/core/DownloadManagerNative.cpp
        src/core/WebProfileWindows.cpp
        src/core/WebViewWindows.cpp
    )
elseif(APPLE)
    target_sources(zenith-browser PRIVATE
        src/core/DownloadManagerNative.cpp
        src/core/WebProfileMac.mm
        src/core/WebViewMac.mm
    )
endif()

target_include_directories(zenith-browser PRIVATE src)
target_compile_definitions(zenith-browser PRIVATE ZENITH_VERSION="${ZENITH_RELEASE_VERSION}")
if(WIN32 AND CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64" AND QT_HOST_PATH)
    set_target_properties(zenith-browser PROPERTIES
        AUTOMOC_EXECUTABLE "${QT_HOST_PATH}/bin/moc.exe"
        AUTORCC_EXECUTABLE "${QT_HOST_PATH}/bin/rcc.exe"
        AUTOUIC_EXECUTABLE "${QT_HOST_PATH}/bin/uic.exe"
    )
endif()

target_link_libraries(zenith-browser PRIVATE
    Qt6::Widgets
    Qt6::Svg
    Qt6::Network
    Qt6::Concurrent
    Qt6::Sql
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(zenith-browser PRIVATE Qt6::DBus PkgConfig::WEBKITGTK X11::X11 ${CMAKE_DL_LIBS})
    target_link_options(zenith-browser PRIVATE "LINKER:--export-dynamic-symbol=gtk_clipboard_request_targets")
elseif(WIN32)
    target_include_directories(zenith-browser PRIVATE "${WEBVIEW2_INCLUDE_DIR}")
    target_link_libraries(zenith-browser PRIVATE Advapi32 Ole32 Shlwapi Version "${WEBVIEW2_LOADER_LIBRARY}")
    set_target_properties(zenith-browser PROPERTIES WIN32_EXECUTABLE TRUE)
elseif(APPLE)
    target_link_libraries(zenith-browser PRIVATE "${WEBKIT_FRAMEWORK}" "${APPKIT_FRAMEWORK}" "${SECURITY_FRAMEWORK}")
    set_target_properties(zenith-browser PROPERTIES MACOSX_BUNDLE TRUE)
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT ZENITH_IPO_SUPPORTED OUTPUT ZENITH_IPO_ERROR)
if(ZENITH_IPO_SUPPORTED)
    set_property(TARGET zenith-browser PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()
if(NOT MSVC)
    target_link_options(zenith-browser PRIVATE "$<$<CONFIG:Release>:-s>")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    pkg_check_modules(LIBSECRET REQUIRED IMPORTED_TARGET libsecret-1)
    target_link_libraries(zenith-browser PRIVATE PkgConfig::LIBSECRET)
endif()

include(CTest)
if(BUILD_TESTING AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
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
        src/core/DownloadManagerShared.cpp
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
if(WIN32)
    if(CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
        foreach(ZENITH_QTPATHS_NAME zenith-qtpaths.bat qtpaths6.bat qtpaths.bat)
            if(EXISTS "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}/${ZENITH_QTPATHS_NAME}")
                set(ZENITH_TARGET_QTPATHS "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}/${ZENITH_QTPATHS_NAME}")
                break()
            endif()
        endforeach()
        if(NOT ZENITH_TARGET_QTPATHS)
            message(FATAL_ERROR "ARM64 Qt cross-deployment requires a target qtpaths batch file in ${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}")
        endif()
        qt_generate_deploy_script(TARGET zenith-browser OUTPUT_SCRIPT ZENITH_DEPLOY_SCRIPT
            CONTENT "set(__QT_DEPLOY_TARGET_QT_PATHS_PATH \"${ZENITH_TARGET_QTPATHS}\")\nqt6_deploy_runtime_dependencies(\n    EXECUTABLE \"$<TARGET_FILE:zenith-browser>\"\n    GENERATE_QT_CONF\n)\n")
    else()
        qt_generate_deploy_app_script(TARGET zenith-browser OUTPUT_SCRIPT ZENITH_DEPLOY_SCRIPT)
    endif()
    install(SCRIPT "${ZENITH_DEPLOY_SCRIPT}")
endif()
install(FILES packaging/zenith-browser.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES resources/icons/zenith.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps RENAME zenith-browser.svg)
install(FILES resources/icons/zenith.png DESTINATION ${CMAKE_INSTALL_DATADIR}/pixmaps RENAME zenith-browser.png)
install(FILES README.md DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/zenith-browser)
install(FILES docs/VALIDATION.md docs/ROADMAP.md docs/OUTLOOK-COMPATIBILITY.md docs/MEMORY.md docs/ADBLOCK.md docs/SECURITY.md docs/APPEARANCE.md docs/RELEASE.md docs/PLANO-1.0.md docs/PREFERENCES.md docs/EXECUCAO-1.0.md docs/PLATAFORMAS.md DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/zenith-browser/docs)
set(CPACK_PACKAGE_VERSION ${ZENITH_RELEASE_VERSION})
set(CPACK_PACKAGE_CONTACT "Zenith Browser")
if(WIN32)
    if(CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
        set(ZENITH_WINDOWS_PACKAGE_ARCH arm64)
    else()
        set(ZENITH_WINDOWS_PACKAGE_ARCH x64)
    endif()
    set(CPACK_GENERATOR "NSIS;WIX")
    set(CPACK_PACKAGE_FILE_NAME "ZenithBrowser-${ZENITH_RELEASE_VERSION}-Windows-${ZENITH_WINDOWS_PACKAGE_ARCH}")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "Zenith Browser")
    set(CPACK_PACKAGE_EXECUTABLES "zenith-browser" "Zenith Browser")
    set(CPACK_NSIS_DISPLAY_NAME "Zenith Browser")
    set(CPACK_NSIS_PACKAGE_NAME "Zenith Browser")
    set(CPACK_NSIS_MUI_FINISHPAGE_RUN "bin/zenith-browser.exe")
    set(CPACK_WIX_UPGRADE_GUID "D4D8499B-2C58-4A89-AC11-38F86F39DF16")
    set(CPACK_WIX_PRODUCT_VERSION "${ZENITH_RELEASE_VERSION}")
    set(CPACK_WIX_PRODUCT_NAME "Zenith Browser")
    set(CPACK_WIX_INSTALL_SCOPE "perMachine")
    set(CPACK_WIX_ARCHITECTURE "${ZENITH_WINDOWS_PACKAGE_ARCH}")
else()
    set(CPACK_GENERATOR TGZ)
endif()
include(CPack)
