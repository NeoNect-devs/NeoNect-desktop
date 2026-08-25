cmake_minimum_required(VERSION 3.16)

set(CMAKE_AUTOMOC_COMPILER_PREDEFS OFF)

project(Avila VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)



# =====================================================================
# 1. Find and load the Qt6 package natively
# =====================================================================
find_package(Qt6 REQUIRED COMPONENTS Gui Qml Quick Concurrent Network)

# =====================================================================
# 2. Fix Qt 6.9+ Infinite Recursion Policy Bug
# =====================================================================
qt_standard_project_setup(REQUIRES 6.9)

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
qt_add_executable(AvilaApp
    src/main.cpp
    src/themedata.h
    src/core/networkmanager.h src/core/networkmanager.cpp
    src/core/cryptomanager.h src/core/cryptomanager.cpp
    src/core/chatmessagemodel.h src/core/chatmessagemodel.cpp
)
set_target_properties(AvilaApp PROPERTIES AUTOMOC_COMPILER_PREDEFS OFF)


# =====================================================================
# 5. Process the QML modules (Explicit List with SVG Assets)
# =====================================================================
qt_add_qml_module(AvilaApp
    URI Avila
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
        qml/components/AvilaButton.qml
        qml/components/AvilaMessageInput.qml
        qml/components/AvilaTextField.qml
        qml/components/ChannelListItem.qml
        qml/components/CircularImage.qml
        qml/components/ScrollableArea.qml
        qml/components/AvilaBrandButton.qml
        # --- Entrypage Folder ---
        qml/entrypage/entry.qml

        # --- SideSection Folder ---
        qml/SideSection/SidebarCanvas.qml

    RESOURCES
        assets/logo.png
        assets/icons/arrow-back.svg
        assets/icons/friends.svg
        assets/icons/hash.svg
        assets/icons/headphones.svg
        assets/icons/mic.svg
        assets/icons/plus-circle.svg
        assets/icons/send.svg
        assets/icons/settings.svg
        assets/icons/smile.svg
        assets/icons/sticker.svg
        assets/icons/users.svg
)

target_include_directories(AvilaApp PRIVATE src)

# =====================================================================
# 6. Pipeline Linking Operations
# =====================================================================
target_link_libraries(AvilaApp PRIVATE
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Concurrent
    Qt6::Network
    OpenSSL::SSL
    OpenSSL::Crypto
)

# =====================================================================
# 7. Automatic DLL Deployment (Windows Only)
# =====================================================================
if(WIN32)
    set(OPENSSL_BIN_DIR "${OPENSSL_ROOT_DIR}/bin")
    find_program(WINDEPLOYQT_EXEC windeployqt HINTS "C:/Qt/6.11.2/mingw_64/bin")

    if(WINDEPLOYQT_EXEC)
        add_custom_command(TARGET AvilaApp POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${OPENSSL_BIN_DIR}/libcrypto-4-x64.dll"
                "${OPENSSL_BIN_DIR}/libssl-4-x64.dll"
                $<TARGET_FILE_DIR:AvilaApp>
            COMMAND "${WINDEPLOYQT_EXEC}" --no-compiler-runtime --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml" "$<TARGET_FILE:AvilaApp>"
            COMMENT "➔ Deploying OpenSSL and Qt runtime DLLs via windeployqt..."
        )
    else()
        add_custom_command(TARGET AvilaApp POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${OPENSSL_BIN_DIR}/libcrypto-4-x64.dll"
                "${OPENSSL_BIN_DIR}/libssl-4-x64.dll"
                $<TARGET_FILE_DIR:AvilaApp>
            COMMENT "➔ Automatically deploying OpenSSL 4 runtime DLLs to target execution directory..."
        )
    endif()
endif()