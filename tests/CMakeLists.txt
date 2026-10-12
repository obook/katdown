find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test Network)

set(KATEMARK_TEST_LIBRARIES
    Qt6::Test
    Qt6::Network
    Qt6::Widgets
    Qt6::WebEngineWidgets
    KF6::TextEditor
    KF6::I18n
    KF6::ConfigCore
    KF6::XmlGui
    KF6::SyntaxHighlighting
)

# The preview's sources live in a plugin, which a program cannot link against. They are
# compiled once here, as an object library, and shared by the test programs.
add_library(katemarktestcore OBJECT
    ../src/previewwidget.cpp
    ../src/previewtheme.cpp
    ../src/previewload.cpp
    ../src/previewexport.cpp
    ../src/previewscroll.cpp
    ../src/previewinput.cpp
    ../src/imagepaste.cpp
    ../src/markdownedit.cpp
    ../src/markdowntable.cpp
    ../src/settings.cpp
    ../data/resources.qrc
)
target_include_directories(katemarktestcore PUBLIC ../src)
target_link_libraries(katemarktestcore PUBLIC ${KATEMARK_TEST_LIBRARIES})

# The XDG folders keep the tests' config out of the developer's own Kate settings.
set(KATEMARK_TEST_ENVIRONMENT
    "QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu --no-sandbox"
    "XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/xdg/config"
    "XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/xdg/cache"
    "XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/xdg/data"
)

# QtWebEngine does not start under the offscreen platform on every Qt build, so
# it is asked for only when there is no display to use.
if(NOT DEFINED ENV{DISPLAY} AND NOT DEFINED ENV{WAYLAND_DISPLAY})
    list(APPEND KATEMARK_TEST_ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

foreach(test previewlifecycletest renderingtest interactiontest exporttest editingtest)
    add_executable(${test} ${test}.cpp)
    target_link_libraries(${test} PRIVATE katemarktestcore)
    add_test(NAME ${test} COMMAND ${test})

    set_tests_properties(${test} PROPERTIES
        TIMEOUT 300
        ENVIRONMENT "${KATEMARK_TEST_ENVIRONMENT}"
    )
endforeach()
