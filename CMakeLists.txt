cmake_minimum_required(VERSION 3.16)

project(Avila VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_PREFIX_PATH "C:/Qt/6.9.1/mingw_64/lib/cmake")

find_package(Qt6 REQUIRED COMPONENTS Quick)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(Avila
    main.cpp
)

qt_add_qml_module(Avila
    URI Avila
    VERSION 1.0
    QML_FILES
        Main.qml
        QML_FILES SidebarCanvas.qml
        QML_FILES TopbarCanvas.qml
        QML_FILES MemberListCanvas.qml
        QML_FILES ChatboxCanvas.qml
        QML_FILES MessageInputCanvas.qml
        QML_FILES MainPanel.qml
        QML_FILES MessageBubble.qml
        QML_FILES MessagesGroup.qml
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(Avila PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.Avila
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(Avila
    PRIVATE Qt6::Quick
)

include(GNUInstallDirs)
install(TARGETS Avila
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
