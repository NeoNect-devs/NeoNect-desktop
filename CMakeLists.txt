cmake_minimum_required(VERSION 3.16)

project(Avila VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_PREFIX_PATH "C:/Qt/6.9.1/mingw_64/lib/cmake")

find_package(Qt6 REQUIRED COMPONENTS Quick)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(Avila
    main.cpp
)

set_source_files_properties(qml/modules/ThemeData/ThemeData.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE)


set(QT_QML_SKIP_QMLDIR_ENTRY FALSE)
qt_add_qml_module(Avila
    URI Avila
    VERSION 1.0

        QML_FILES qml/Main.qml


        QML_FILES qml/modules/ThemeData/ThemeData.qml

        QML_FILES qml/MainSection/MainPanel.qml
        QML_FILES qml/MainSection/TopView/TopbarCanvas.qml

        QML_FILES qml/MainSection/MemberListView/MemberListCanvas.qml
        QML_FILES qml/MainSection/MemberListView/MemberItem.qml

        QML_FILES qml/MainSection/ChatView/ChatboxCanvas.qml
        QML_FILES qml/MainSection/ChatView/MessageBubble.qml
        QML_FILES qml/MainSection/ChatView/CircularImage.qml
        QML_FILES qml/MainSection/ChatView/MessageInputCanvas.qml
        QML_FILES qml/MainSection/ChatView/ChatHandler.js

        QML_FILES qml/SideSection/SidebarCanvas.qml
        QML_FILES qml/entrypage/entry.qml

        RESOURCES
            assets/icons/arrow-back.svg
            assets/logo.png
            QML_FILES qml/entrypage/apiService.js
            QML_FILES qml/SideSection/ChannelListItem.qml



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
