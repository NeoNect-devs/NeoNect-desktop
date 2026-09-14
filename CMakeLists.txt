cmake_minimum_required(VERSION 3.16)
set(CMAKE_AUTOMOC_COMPILER_PREDEFS OFF CACHE BOOL "Disable predefs" FORCE)
set(CMAKE_AUTOMOC_COMPILER_PREDEFS OFF)

project(NeoNect VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =====================================================================
# 1. Find and load the Qt6 package natively
# =====================================================================
find_package(Qt6 REQUIRED COMPONENTS Gui Qml Quick Concurrent Network Test Sql)

# =====================================================================
# 2. Fix Qt 6.9+ Infinite Recursion Policy Bug
# =====================================================================
qt_standard_project_setup(REQUIRES 6.9)
set(CMAKE_CXX_COMPILER_PREDEFINES_COMMAND "")
set(CMAKE_C_COMPILER_PREDEFINES_COMMAND "")
set(CMAKE_AUTOMOC_COMPILER_PREDEFS OFF)

# =====================================================================
# 3. OpenSSL 4.0.0 Windows Multi-Path Discovery & Target Bypass
# =====================================================================
if(WIN32)
    set(TEST_PATHS
        "C:/OpenSSL-Win64"
        "C:/PROGRA~1/OpenSSL-Win64"
        "C:/Program Files/OpenSSL-Win64"
        "C:/OpenSSL"
    )

    set(OPENSSL_ROOT_DIR "")
    foreach(PATH_CANDIDATE ${TEST_PATHS})
        if(EXISTS "${PATH_CANDIDATE}")
            set(OPENSSL_ROOT_DIR "${PATH_CANDIDATE}")
            message(STATUS "➔ Found OpenSSL Root Directory at: ${OPENSSL_ROOT_DIR}")
            break()
        endif()
    endforeach()

    if("${OPENSSL_ROOT_DIR}" STREQUAL "")
        message(FATAL_ERROR "➔ OpenSSL installation folder not found!")
    endif()

    set(OPENSSL_INCLUDE_DIR "${OPENSSL_ROOT_DIR}/include")

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
        add_library(OpenSSL::Crypto UNKNOWN IMPORTED)
        set_target_properties(OpenSSL::Crypto PROPERTIES
            INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
            INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
            IMPORTED_LOCATION "${MANUAL_CRYPTO_LIB}"
        )

        add_library(OpenSSL::SSL UNKNOWN IMPORTED)
        set_target_properties(OpenSSL::SSL PROPERTIES
            INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
            INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
            IMPORTED_LOCATION "${MANUAL_SSL_LIB}"
            INTERFACE_LINK_LIBRARIES OpenSSL::Crypto
        )
    else()
        message(FATAL_ERROR "➔ Failed to bind OpenSSL runtime files.")
    endif()
else()
    find_package(OpenSSL REQUIRED)
endif()

# =====================================================================
# 4. Define the executable target (C++ ONLY)
# =====================================================================
qt_add_executable(NeoNectApp
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
    tests/mocks/mockhttptransport.h tests/mocks/mockhttptransport.cpp

    # Services
    src/services/authservice.h src/services/authservice.cpp
    src/services/deviceservice.h src/services/deviceservice.cpp
    src/services/relayservice.h src/services/relayservice.cpp
    src/services/friendservice.h src/services/friendservice.cpp

    # Core & Facade
    src/core/application.h src/core/application.cpp
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
        assets/logo.png
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
)

set_target_properties(NeoNectApp PROPERTIES AUTOMOC_COMPILER_PREDEFS OFF)
target_include_directories(NeoNectApp PRIVATE src tests)

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

    # Services
    src/services/authservice.h src/services/authservice.cpp
    src/services/deviceservice.h src/services/deviceservice.cpp
    src/services/relayservice.h src/services/relayservice.cpp
    src/services/friendservice.h src/services/friendservice.cpp

    # Core & Facade
    src/core/networkmanager.h src/core/networkmanager.cpp
    src/core/cryptomanager.h src/core/cryptomanager.cpp
    src/core/chatmessagemodel.h src/core/chatmessagemodel.cpp
    src/core/audiomanager.h src/core/audiomanager.cpp
    src/core/notificationmanager.h src/core/notificationmanager.cpp
)

set_target_properties(NeoNectTests PROPERTIES AUTOMOC ON AUTOMOC_COMPILER_PREDEFS OFF)
target_include_directories(NeoNectTests PRIVATE src tests)
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
    src/services/authservice.h src/services/authservice.cpp
    src/services/deviceservice.h src/services/deviceservice.cpp
    src/services/relayservice.h src/services/relayservice.cpp
    src/services/friendservice.h src/services/friendservice.cpp
    src/core/networkmanager.h src/core/networkmanager.cpp
    src/core/cryptomanager.h src/core/cryptomanager.cpp
    src/core/chatmessagemodel.h src/core/chatmessagemodel.cpp
    src/core/audiomanager.h src/core/audiomanager.cpp
    src/core/notificationmanager.h src/core/notificationmanager.cpp
)
set_target_properties(NeoNectStressTests PROPERTIES AUTOMOC ON AUTOMOC_COMPILER_PREDEFS OFF)
target_include_directories(NeoNectStressTests PRIVATE src tests)
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
    src/services/authservice.h src/services/authservice.cpp
    src/services/deviceservice.h src/services/deviceservice.cpp
    src/services/relayservice.h src/services/relayservice.cpp
    src/services/friendservice.h src/services/friendservice.cpp
    src/core/networkmanager.h src/core/networkmanager.cpp
    src/core/cryptomanager.h src/core/cryptomanager.cpp
    src/core/chatmessagemodel.h src/core/chatmessagemodel.cpp
    src/core/audiomanager.h src/core/audiomanager.cpp
    src/core/notificationmanager.h src/core/notificationmanager.cpp
)
set_target_properties(NeoNectBenchmarks PROPERTIES AUTOMOC ON AUTOMOC_COMPILER_PREDEFS OFF)
target_include_directories(NeoNectBenchmarks PRIVATE src tests)
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