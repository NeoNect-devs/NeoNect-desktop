cmake_minimum_required(VERSION 3.16)
set(CMAKE_AUTOMOC_COMPILER_PREDEFS OFF CACHE BOOL "Disable predefs" FORCE)
set(CMAKE_AUTOMOC_COMPILER_PREDEFS OFF)

project(NeoNect VERSION 1.0 LANGUAGES CXX)

# =====================================================================
# Automated Semantic Versioning & Metadata Extraction
# =====================================================================
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Version.cmake)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/src/version.h
    @ONLY
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =====================================================================
# 1. Find and load the Qt6 package natively
# =====================================================================
find_package(Qt6 REQUIRED COMPONENTS Gui Qml Quick Concurrent Network Test Sql)

# =====================================================================
# 2. Fix Qt 6.9+ Infinite Recursion Policy Bug
# =====================================================================
qt_standard_project_setup(REQUIRES 6.5)
set(CMAKE_CXX_COMPILER_PREDEFINES_COMMAND "")
set(CMAKE_C_COMPILER_PREDEFINES_COMMAND "")
set(QT_NO_AOT_COMPILATION ON CACHE BOOL "Disable QML AOT compiler" FORCE)
set(QT_NO_CREATE_AOT_PLUGINS ON CACHE BOOL "Disable QML AOT plugins" FORCE)

# =====================================================================
# 3. OpenSSL 4.0.0 Windows Multi-Path Discovery & Target Bypass
# =====================================================================
if(WIN32)
    set(TEST_PATHS
        "$ENV{OPENSSL_ROOT_DIR}"
        "C:/OpenSSL"
        "C:/OpenSSL-Win64"
        "C:/PROGRA~1/OpenSSL"
        "C:/PROGRA~1/OpenSSL-Win64"
        "C:/Program Files/OpenSSL"
        "C:/Program Files/OpenSSL-Win64"
    )

    set(RESOLVED_OPENSSL_ROOT "")
    foreach(PATH_CANDIDATE ${TEST_PATHS})
        if(PATH_CANDIDATE AND EXISTS "${PATH_CANDIDATE}")
            set(RESOLVED_OPENSSL_ROOT "${PATH_CANDIDATE}")
            message(STATUS "➔ Found OpenSSL Root Directory at: ${RESOLVED_OPENSSL_ROOT}")
            break()
        endif()
    endforeach()

    if(RESOLVED_OPENSSL_ROOT MATCHES " ")
        file(TO_NATIVE_PATH "${RESOLVED_OPENSSL_ROOT}" NATIVE_OPENSSL_ROOT)
        execute_process(
            COMMAND cmd.exe /c for %I in ("${NATIVE_OPENSSL_ROOT}") do @echo %~sI
            OUTPUT_VARIABLE SHORT_OPENSSL_ROOT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(SHORT_OPENSSL_ROOT AND EXISTS "${SHORT_OPENSSL_ROOT}")
            file(TO_CMAKE_PATH "${SHORT_OPENSSL_ROOT}" RESOLVED_OPENSSL_ROOT)
            message(STATUS "➔ Converted OpenSSL path to 8.3 short format: ${RESOLVED_OPENSSL_ROOT}")
        endif()
    endif()

    if("${RESOLVED_OPENSSL_ROOT}" STREQUAL "")
        find_package(OpenSSL)
        if(NOT OPENSSL_FOUND)
            message(FATAL_ERROR "➔ OpenSSL installation folder not found!")
        endif()
    else()
        set(OPENSSL_ROOT_DIR "${RESOLVED_OPENSSL_ROOT}" CACHE PATH "OpenSSL Root Directory" FORCE)
        set(OPENSSL_INCLUDE_DIR "${OPENSSL_ROOT_DIR}/include" CACHE PATH "OpenSSL Include Directory" FORCE)

        if(OPENSSL_INCLUDE_DIR MATCHES " ")
            file(TO_NATIVE_PATH "${OPENSSL_INCLUDE_DIR}" NATIVE_INC)
            execute_process(
                COMMAND cmd.exe /c "for %I in (\"${NATIVE_INC}\") do @echo %~sI"
                OUTPUT_VARIABLE SHORT_INC
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
            )
            if(SHORT_INC AND EXISTS "${SHORT_INC}")
                file(TO_CMAKE_PATH "${SHORT_INC}" OPENSSL_INCLUDE_DIR)
            endif()
        endif()

        file(GLOB_RECURSE FOUND_CRYPTO_FILES "${OPENSSL_ROOT_DIR}/*crypto*.*")
        file(GLOB_RECURSE FOUND_SSL_FILES    "${OPENSSL_ROOT_DIR}/*ssl*.*")

        set(MANUAL_CRYPTO_LIB "")
        foreach(FILE ${FOUND_CRYPTO_FILES})
            if(NOT FILE MATCHES ".*/bin/.*" AND (FILE MATCHES ".*\\.lib$" OR FILE MATCHES ".*\\.a$"))
                set(MANUAL_CRYPTO_LIB "${FILE}")
                break()
            endif()
        endforeach()

        set(MANUAL_SSL_LIB "")
        foreach(FILE ${FOUND_SSL_FILES})
            if(NOT FILE MATCHES ".*/bin/.*" AND (FILE MATCHES ".*\\.lib$" OR FILE MATCHES ".*\\.a$"))
                set(MANUAL_SSL_LIB "${FILE}")
                break()
            endif()
        endforeach()

        if(MANUAL_CRYPTO_LIB AND MANUAL_SSL_LIB)
            if(NOT TARGET OpenSSL::Crypto)
                add_library(OpenSSL::Crypto UNKNOWN IMPORTED)
                set_target_properties(OpenSSL::Crypto PROPERTIES
                    INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
                    INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
                    IMPORTED_LOCATION "${MANUAL_CRYPTO_LIB}"
                )
            endif()

            if(NOT TARGET OpenSSL::SSL)
                add_library(OpenSSL::SSL UNKNOWN IMPORTED)
                set_target_properties(OpenSSL::SSL PROPERTIES
                    INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
                    INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
                    IMPORTED_LOCATION "${MANUAL_SSL_LIB}"
                    INTERFACE_LINK_LIBRARIES OpenSSL::Crypto
                )
            endif()
        else()
            find_package(OpenSSL REQUIRED)
        endif()
    endif()
else()
    find_package(OpenSSL REQUIRED)
endif()

# =====================================================================
# 4. Define the executable target (C++ ONLY)
# =====================================================================
set(APP_ICON_RESOURCE "")
if(WIN32)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/NeoNect.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/NeoNect.rc
        @ONLY
    )
    set(APP_ICON_RESOURCE "${CMAKE_CURRENT_BINARY_DIR}/NeoNect.rc")
    set_source_files_properties(${APP_ICON_RESOURCE} PROPERTIES
        LANGUAGE RC
        INCLUDE_DIRECTORIES "${CMAKE_CURRENT_BINARY_DIR}"
    )
endif()

qt_add_executable(NeoNectApp
    ${APP_ICON_RESOURCE}
    src/main.cpp
    src/themedata.h

    # Common
    src/common/types.h
    src/common/constants.h
    src/common/openssl_raii.h

    # Cryptography
    src/crypto/icryptoservice.h
    src/crypto/cryptoservice.h src/crypto/cryptoservice.cpp
    src/services/messageservice.h src/services/messageservice.cpp

    # Storage
    src/storage/isettingsrepository.h
    src/storage/settingsrepository.h src/storage/settingsrepository.cpp
    src/storage/sqlmessagerepository.h src/storage/sqlmessagerepository.cpp

    # Transport
    src/transport/ihttptransport.h
    src/transport/httptransport.h src/transport/httptransport.cpp
    src/transport/websocketclient.h src/transport/websocketclient.cpp
    tests/mocks/mockhttptransport.h tests/mocks/mockhttptransport.cpp

    # Services
    src/services/authservice.h src/services/authservice.cpp
    src/services/deviceservice.h src/services/deviceservice.cpp
    src/services/relayservice.h src/services/relayservice.cpp
    src/services/friendservice.h src/services/friendservice.cpp

    # Core & Facade
    src/core/application.h src/core/application.cpp
    src/core/versioninfo.h src/core/versioninfo.cpp
    src/core/networkmanager.h src/core/networkmanager.cpp
    src/core/cryptomanager.h src/core/cryptomanager.cpp
    src/core/chatmessagemodel.h src/core/chatmessagemodel.cpp
    src/core/audiomanager.h src/core/audiomanager.cpp
    src/core/notificationmanager.h src/core/notificationmanager.cpp
)
set_target_properties(NeoNectApp PROPERTIES AUTOMOC_COMPILER_PREDEFS OFF)


# =====================================================================
# 5. Process the QML modules (Explicit List with SVG Assets)
# =====================================================================
qt_add_qml_module(NeoNectApp
    URI NeoNect
    VERSION 1.0
    NO_CACHEGEN
    QML_FILES
        # --- Root QML Folder ---
        qml/Main.qml
        qml/MainPanel.qml

        # --- Containers Folder ---
        qml/containers/WindowTitleBar.qml
        qml/containers/ChannelSidebarPanel.qml
        qml/containers/MembersSidebarPanel.qml
        qml/containers/UserProfileFooter.qml

        # --- Components Folder ---
        qml/components/UIHelpers.js
        qml/components/ChatView.qml
        qml/components/ChatHeader.qml
        qml/components/MessageDelegate.qml
        qml/components/NeoNectButton.qml
        qml/components/NeoNectMessageInput.qml
        qml/components/NeoNectTextField.qml
        qml/components/ChannelListItem.qml
        qml/components/CircularImage.qml
        qml/components/ScrollableArea.qml
        qml/components/NeoNectBrandButton.qml
        qml/components/StickerPickerPanel.qml
        qml/components/VoiceRecorderBar.qml
        qml/components/VoiceMessagePlayer.qml
        qml/components/AudioMusicPlayer.qml
        qml/components/VideoPlayerItem.qml
        qml/components/MediaLightboxModal.qml
        qml/components/VolumeController.qml
        qml/components/GradientSeekBar.qml
        qml/components/ScrollToBottomButton.qml
        qml/components/ToastNotification.qml
        qml/components/FriendsHomePanel.qml
        qml/components/NotificationStackView.qml
        qml/components/NotificationCenterFlyout.qml
        qml/components/SettingsProfileModal.qml

        # --- Entrypage Folder ---
        qml/entrypage/entry.qml

        # --- SideSection Folder ---
        qml/SideSection/SidebarCanvas.qml

    RESOURCES
        assets/NeoNect/icon.png
        assets/NeoNect/icon.ico
        assets/icons/arrow-back.svg
        assets/icons/friends.svg
        assets/icons/bookmark.svg
        assets/icons/check.svg
        assets/icons/check-check.svg
        assets/icons/search.svg
        assets/icons/chat.svg
        assets/icons/hash.svg
        assets/icons/headphones.svg
        assets/icons/mic.svg
        assets/icons/plus-circle.svg
        assets/icons/send.svg
        assets/icons/settings.svg
        assets/icons/user.svg
        assets/icons/palette.svg
        assets/icons/bell.svg
        assets/icons/lock.svg
        assets/icons/shield.svg
        assets/icons/log-out.svg
        assets/icons/smile.svg
        assets/icons/sticker.svg
        assets/icons/users.svg
        assets/icons/play.svg
        assets/icons/pause.svg
        assets/icons/trash.svg
        assets/icons/file.svg
        assets/icons/image.svg
        assets/icons/video.svg
        assets/icons/music.svg
        assets/icons/download.svg
        assets/icons/alert-circle.svg
        assets/icons/refresh.svg
        assets/icons/volume-2.svg
        assets/icons/volume-x.svg
        assets/icons/maximize.svg
        assets/icons/minimize.svg
        # Stickers: Duck Pack
        assets/stickers/duck/duck_happy.svg
        assets/stickers/duck/duck_love.svg
        assets/stickers/duck/duck_cool.svg
        assets/stickers/duck/duck_party.svg
        assets/stickers/duck/duck_thinking.svg
        assets/stickers/duck/duck_angry.svg
        assets/stickers/duck/duck_sad.svg
        assets/stickers/duck/duck_thumbsup.svg
        # Stickers: Crypto Pack
        assets/stickers/crypto/crypto_rocket.svg
        assets/stickers/crypto/crypto_hodl.svg
        assets/stickers/crypto/crypto_fire.svg
        assets/stickers/crypto/crypto_diamond.svg
        assets/stickers/crypto/crypto_popcorn.svg
        assets/stickers/crypto/crypto_cheers.svg
        assets/stickers/crypto/crypto_brain.svg
        assets/stickers/crypto/crypto_hacker.svg
        # Stickers: Pepe Pack
        assets/stickers/pepe/pepe_coffee.svg
        assets/stickers/pepe/pepe_ok.svg
        assets/stickers/pepe/pepe_sunglasses.svg
        assets/stickers/pepe/pepe_smart.svg
        assets/stickers/pepe/pepe_gamer.svg
        assets/stickers/pepe/pepe_cry.svg
        # In-App Release Notes
        assets/patch_notes.json
)

set_target_properties(NeoNectApp PROPERTIES AUTOMOC_COMPILER_PREDEFS OFF)
target_include_directories(NeoNectApp PRIVATE src tests ${CMAKE_CURRENT_BINARY_DIR}/src)

# =====================================================================
# 6. Pipeline Linking Operations
# =====================================================================
target_link_libraries(NeoNectApp PRIVATE
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Concurrent
    Qt6::Network
    Qt6::Sql
    OpenSSL::SSL
    OpenSSL::Crypto
)
if(WIN32)
    target_link_libraries(NeoNectApp PRIVATE winmm dwmapi)
endif()

# =====================================================================
# 7. Automatic DLL Deployment (Windows Only)
# =====================================================================
if(WIN32)
    set(OPENSSL_BIN_DIR "${OPENSSL_ROOT_DIR}/bin")
    set(MINGW_BIN_DIR "C:/Qt/Tools/mingw1310_64/bin")
    find_program(WINDEPLOYQT_EXEC windeployqt HINTS "C:/Qt/6.11.2/mingw_64/bin")

    if(WINDEPLOYQT_EXEC)
        add_custom_command(TARGET NeoNectApp POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${OPENSSL_BIN_DIR}/libcrypto-4-x64.dll"
                "${OPENSSL_BIN_DIR}/libssl-4-x64.dll"
                "${MINGW_BIN_DIR}/libstdc++-6.dll"
                "${MINGW_BIN_DIR}/libgcc_s_seh-1.dll"
                "${MINGW_BIN_DIR}/libwinpthread-1.dll"
                $<TARGET_FILE_DIR:NeoNectApp>
            COMMAND "${WINDEPLOYQT_EXEC}" --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml" "$<TARGET_FILE:NeoNectApp>"
            COMMENT "➔ Deploying OpenSSL, MinGW C++ runtime, and Qt DLLs via windeployqt..."
        )
    else()
        add_custom_command(TARGET NeoNectApp POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${OPENSSL_BIN_DIR}/libcrypto-4-x64.dll"
                "${OPENSSL_BIN_DIR}/libssl-4-x64.dll"
                "${MINGW_BIN_DIR}/libstdc++-6.dll"
                "${MINGW_BIN_DIR}/libgcc_s_seh-1.dll"
                "${MINGW_BIN_DIR}/libwinpthread-1.dll"
                $<TARGET_FILE_DIR:NeoNectApp>
            COMMENT "➔ Deploying OpenSSL and MinGW C++ runtime DLLs..."
        )
    endif()
endif()

# =====================================================================
# 8. Automated Test Suites (QtTest + CTest)
# =====================================================================
enable_testing()

add_executable(NeoNectTests
    tests/main_test.cpp
    tests/test_crypto.h tests/test_crypto.cpp
    tests/test_storage.h tests/test_storage.cpp
    tests/test_models.h tests/test_models.cpp
    tests/test_services.h tests/test_services.cpp
    tests/test_messages.h tests/test_messages.cpp
    tests/mocks/mockhttptransport.h tests/mocks/mockhttptransport.cpp

    # Common
    src/common/types.h
    src/common/constants.h
    src/common/openssl_raii.h

    # Cryptography
    src/crypto/icryptoservice.h
    src/crypto/cryptoservice.h src/crypto/cryptoservice.cpp
    src/services/messageservice.h src/services/messageservice.cpp

    # Storage
    src/storage/isettingsrepository.h
    src/storage/settingsrepository.h src/storage/settingsrepository.cpp
    src/storage/sqlmessagerepository.h src/storage/sqlmessagerepository.cpp

    # Transport
    src/transport/ihttptransport.h
    src/transport/httptransport.h src/transport/httptransport.cpp
    src/transport/websocketclient.h src/transport/websocketclient.cpp

    # Services
    src/services/authservice.h src/services/authservice.cpp
    src/services/deviceservice.h src/services/deviceservice.cpp
    src/services/relayservice.h src/services/relayservice.cpp
    src/services/friendservice.h src/services/friendservice.cpp

    # Core & Facade
    src/core/versioninfo.h src/core/versioninfo.cpp
    src/core/networkmanager.h src/core/networkmanager.cpp
    src/core/cryptomanager.h src/core/cryptomanager.cpp
    src/core/chatmessagemodel.h src/core/chatmessagemodel.cpp
    src/core/audiomanager.h src/core/audiomanager.cpp
    src/core/notificationmanager.h src/core/notificationmanager.cpp
)

set_target_properties(NeoNectTests PROPERTIES AUTOMOC ON AUTOMOC_COMPILER_PREDEFS OFF)
target_include_directories(NeoNectTests PRIVATE src tests ${CMAKE_CURRENT_BINARY_DIR}/src)
target_link_libraries(NeoNectTests PRIVATE
    Qt6::Core
    Qt6::Concurrent
    Qt6::Gui
    Qt6::Quick
    Qt6::Network
    Qt6::Sql
    Qt6::Concurrent
    Qt6::Test
    OpenSSL::SSL
    OpenSSL::Crypto
)
if(WIN32)
    target_link_libraries(NeoNectTests PRIVATE winmm dwmapi)
endif()

if(WIN32)
    add_custom_command(TARGET NeoNectTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "C:/Qt/6.11.2/mingw_64/bin/Qt6Test.dll"
            $<TARGET_FILE_DIR:NeoNectTests>
        COMMENT "➔ Deploying Qt6Test.dll for test runner..."
    )
endif()

add_test(NAME NeoNectTestSuite COMMAND NeoNectTests)

# ==============================================================================
# Benchmarks
# ==============================================================================

# ==============================================================================
# Stress Tests
# ==============================================================================
add_executable(NeoNectStressTests tests/stress/main_stress.cpp
    src/common/types.h
    src/common/constants.h
    src/common/openssl_raii.h
    src/crypto/icryptoservice.h
    src/crypto/cryptoservice.h src/crypto/cryptoservice.cpp
    src/services/messageservice.h src/services/messageservice.cpp
    src/storage/isettingsrepository.h
    src/storage/settingsrepository.h src/storage/settingsrepository.cpp
    src/storage/sqlmessagerepository.h src/storage/sqlmessagerepository.cpp
    src/transport/ihttptransport.h
    src/transport/httptransport.h src/transport/httptransport.cpp
    src/transport/websocketclient.h src/transport/websocketclient.cpp
    src/services/authservice.h src/services/authservice.cpp
    src/services/deviceservice.h src/services/deviceservice.cpp
    src/services/relayservice.h src/services/relayservice.cpp
    src/services/friendservice.h src/services/friendservice.cpp
    src/core/versioninfo.h src/core/versioninfo.cpp
    src/core/networkmanager.h src/core/networkmanager.cpp
    src/core/cryptomanager.h src/core/cryptomanager.cpp
    src/core/chatmessagemodel.h src/core/chatmessagemodel.cpp
    src/core/audiomanager.h src/core/audiomanager.cpp
    src/core/notificationmanager.h src/core/notificationmanager.cpp
)
set_target_properties(NeoNectStressTests PROPERTIES AUTOMOC ON AUTOMOC_COMPILER_PREDEFS OFF)
target_include_directories(NeoNectStressTests PRIVATE src tests ${CMAKE_CURRENT_BINARY_DIR}/src)
target_link_libraries(NeoNectStressTests PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Sql
    Qt6::Qml
    Qt6::Quick
    Qt6::Test
    Qt6::Concurrent
    OpenSSL::SSL
    OpenSSL::Crypto
)
option(ENABLE_SANITIZERS "Enable Address and Undefined sanitizers for stress tests" OFF)
if(ENABLE_SANITIZERS)
    if(NOT MINGW)
        target_compile_options(NeoNectStressTests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(NeoNectStressTests PRIVATE -fsanitize=address,undefined)
    else()
        message(WARNING "Sanitizers are not supported on MinGW.")
    endif()
endif()

if(WIN32)
    target_link_libraries(NeoNectStressTests PRIVATE winmm dwmapi)
endif()

add_executable(NeoNectBenchmarks tests/benchmarks/main_benchmark.cpp
    src/common/types.h
    src/common/constants.h
    src/common/openssl_raii.h
    src/crypto/icryptoservice.h
    src/crypto/cryptoservice.h src/crypto/cryptoservice.cpp
    src/services/messageservice.h src/services/messageservice.cpp
    src/storage/isettingsrepository.h
    src/storage/settingsrepository.h src/storage/settingsrepository.cpp
    src/storage/sqlmessagerepository.h src/storage/sqlmessagerepository.cpp
    src/transport/ihttptransport.h
    src/transport/httptransport.h src/transport/httptransport.cpp
    src/transport/websocketclient.h src/transport/websocketclient.cpp
    src/services/authservice.h src/services/authservice.cpp
    src/services/deviceservice.h src/services/deviceservice.cpp
    src/services/relayservice.h src/services/relayservice.cpp
    src/services/friendservice.h src/services/friendservice.cpp
    src/core/versioninfo.h src/core/versioninfo.cpp
    src/core/networkmanager.h src/core/networkmanager.cpp
    src/core/cryptomanager.h src/core/cryptomanager.cpp
    src/core/chatmessagemodel.h src/core/chatmessagemodel.cpp
    src/core/audiomanager.h src/core/audiomanager.cpp
    src/core/notificationmanager.h src/core/notificationmanager.cpp
)
set_target_properties(NeoNectBenchmarks PROPERTIES AUTOMOC ON AUTOMOC_COMPILER_PREDEFS OFF)
target_include_directories(NeoNectBenchmarks PRIVATE src tests ${CMAKE_CURRENT_BINARY_DIR}/src)
target_link_libraries(NeoNectBenchmarks PRIVATE
    Qt6::Core
    Qt6::Concurrent
    Qt6::Network
    Qt6::Sql
    Qt6::Qml
    Qt6::Quick
    Qt6::Test
    OpenSSL::SSL
    OpenSSL::Crypto
)
if(WIN32)
    target_link_libraries(NeoNectBenchmarks PRIVATE winmm dwmapi)
endif()


if(WIN32)
    set_tests_properties(NeoNectTestSuite PROPERTIES ENVIRONMENT "PATH=C:/Qt/6.11.2/mingw_64/bin;C:/Qt/Tools/mingw1310_64/bin;${OPENSSL_ROOT_DIR}/bin;$ENV{PATH}")
endif()

# ==============================================================================
# 9. Installation & CPack Distribution Packaging
# ==============================================================================
include(GNUInstallDirs)

install(TARGETS NeoNectApp
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION .
)

if(UNIX AND NOT APPLE)
    install(FILES packaging/linux/io.neonect.NeoNect.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    )
    install(FILES packaging/linux/io.neonect.NeoNect.metainfo.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo
    )
    install(FILES assets/NeoNect/icon.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
        RENAME io.neonect.NeoNect.png
    )
endif()

set(CPACK_PACKAGE_NAME "NeoNect")
set(CPACK_PACKAGE_VENDOR "NeoNect Devs")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Decentralized E2EE Messenger")
set(CPACK_PACKAGE_VERSION_MAJOR ${NEONECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${NEONECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${NEONECT_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION "${NEONECT_VERSION_STRING}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "NeoNect")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")
    set(CPACK_NSIS_DISPLAY_NAME "NeoNect")
    set(CPACK_NSIS_PACKAGE_NAME "NeoNect")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/NeoNect/icon.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/NeoNect/icon.ico")
elseif(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "TGZ;DEB;RPM")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "NeoNect Devs <contact@neonect.io>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "net")
endif()

include(CPack)

# ==============================================================================
# 10. Doxygen Documentation Target
# ==============================================================================
find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating authentic HTML Doxygen documentation for NeoNect"
        VERBATIM
    )
endif()
