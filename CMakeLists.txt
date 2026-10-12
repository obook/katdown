cmake_minimum_required(VERSION 3.16)

project(katemark VERSION 0.4.5 LANGUAGES CXX)

set(QT_MIN_VERSION 6.5.0)
set(KF_MIN_VERSION 6.0.0)

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    WebEngineWidgets
)

find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
    TextEditor
    CoreAddons
    I18n
    Config
    WidgetsAddons
    XmlGui
    SyntaxHighlighting
)

add_definitions(-DTRANSLATION_DOMAIN="katemark")

kcoreaddons_add_plugin(katemark
    INSTALL_NAMESPACE "kf6/ktexteditor"
    SOURCES
        src/plugin.cpp
        src/pluginview.cpp
        src/pluginviewedit.cpp
        src/previewwidget.cpp
        src/previewtheme.cpp
        src/previewload.cpp
        src/previewexport.cpp
        src/previewscroll.cpp
        src/previewinput.cpp
        src/imagepaste.cpp
        src/markdownedit.cpp
        src/markdowntable.cpp
        src/configpage.cpp
        src/settings.cpp
        data/resources.qrc
)

target_include_directories(katemark PRIVATE src)

target_compile_definitions(katemark PRIVATE
    KATEMARK_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(katemark PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::WebEngineWidgets
    KF6::TextEditor
    KF6::CoreAddons
    KF6::I18n
    KF6::ConfigCore
    KF6::WidgetsAddons
    KF6::XmlGui
    KF6::SyntaxHighlighting
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

ki18n_install(po)

# "cpack -G DEB", run in the build directory, makes an installable package.
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Markdown preview plugin for the Kate editor")
set(CPACK_PACKAGE_CONTACT "Olivier Booklage <olivier.booklage@ac-bordeaux.fr>")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/obook/katemark")
set(CPACK_STRIP_FILES ON)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION "editors")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
include(CPack)
