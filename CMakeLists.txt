cmake_minimum_required(VERSION 3.19)

project(katemark VERSION 0.4.6 LANGUAGES CXX)

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

# The preview, the editing actions and the settings, compiled once for the plugin and the
# test programs: a program cannot link against a plugin.
add_library(katemarkcore OBJECT
    src/previewwidget.cpp
    src/previewtheme.cpp
    src/previewload.cpp
    src/previewexport.cpp
    src/previewscroll.cpp
    src/previewinput.cpp
    src/imagepaste.cpp
    src/markdownedit.cpp
    src/markdowntable.cpp
    src/settings.cpp
    data/resources.qrc
)
set_target_properties(katemarkcore PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(katemarkcore PUBLIC src)
target_link_libraries(katemarkcore PUBLIC
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

kcoreaddons_add_plugin(katemark
    INSTALL_NAMESPACE "kf6/ktexteditor"
    SOURCES
        src/plugin.cpp
        src/pluginview.cpp
        src/pluginviewedit.cpp
        src/configpage.cpp
)

target_compile_definitions(katemark PRIVATE
    KATEMARK_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(katemark PRIVATE katemarkcore)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

ki18n_install(po)

# The changelog of the package opens with the version being built: a release that forgot
# its entry stops here.
file(STRINGS packaging/changelog changelog_head LIMIT_COUNT 1)
if(NOT changelog_head MATCHES "^katemark \\(${PROJECT_VERSION}\\) ")
    message(FATAL_ERROR "packaging/changelog does not start with version ${PROJECT_VERSION}")
endif()
# These files are read here, at configure time: an edit has to configure again.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    packaging/changelog packaging/copyright THIRD-PARTY-NOTICES.md)

# What a Debian package keeps in /usr/share/doc: its changelog, compressed, its copyright
# and the notices of the bundled libraries. Package managers read the changelog there.
# Only the package gets them, not a plain install.
if(UNIX)
    set(package_doc "${CMAKE_BINARY_DIR}/package-doc")
    file(MAKE_DIRECTORY "${package_doc}")
    # gzip -9n, as Debian wants it: no timestamp, so two builds give the same package.
    execute_process(COMMAND gzip -9nc "${CMAKE_SOURCE_DIR}/packaging/changelog"
        OUTPUT_FILE "${package_doc}/changelog.gz" COMMAND_ERROR_IS_FATAL ANY)
    file(COPY packaging/copyright THIRD-PARTY-NOTICES.md DESTINATION "${package_doc}")
    # The package takes the files with the permissions they have here.
    file(CHMOD "${package_doc}/changelog.gz" "${package_doc}/copyright" "${package_doc}/THIRD-PARTY-NOTICES.md"
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
    set(CPACK_INSTALLED_DIRECTORIES "${package_doc}" "usr/share/doc/katemark")
endif()

# "cpack -G DEB", run in the build directory, makes an installable package.
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Markdown preview plugin for the Kate editor")
set(CPACK_PACKAGE_DESCRIPTION
    "Katemark shows the Markdown document you are editing in a panel beside
the editor, as GitHub would. The preview follows what you type and scrolls
with the editor.

It renders math with KaTeX and diagrams with Mermaid, reads the CodiMD,
Obsidian and MkDocs syntax, and exports to PDF and HTML. The renderer, the
styles and the fonts ship with the plugin, so it works offline.")
set(CPACK_PACKAGE_CONTACT "Olivier Booklage <olivier.booklage@ac-bordeaux.fr>")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/obook/katemark")
set(CPACK_STRIP_FILES ON)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION "editors")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
include(CPack)
