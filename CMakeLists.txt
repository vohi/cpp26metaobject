cmake_minimum_required(VERSION 3.16)
project(cpp26metaobject VERSION 0.1 LANGUAGES CXX)

find_package(Qt6 COMPONENTS
    Core Gui Widgets CorePrivate
    REQUIRED
)

find_package(Qt6 OPTIONAL_COMPONENTS
    Quick QuickControls2 QuickWidgets
    Multimedia
    )

if (TARGET Qt6::Quick)
    qt_policy(SET QTP0001 NEW)
endif()

qt_standard_project_setup()

qt_add_executable(cpp26metaobject
    MANUAL_FINALIZATION
    main.cpp
    object.cpp
    user.cpp
)

target_compile_options(cpp26metaobject PRIVATE -fsanitize=address)
target_link_options(cpp26metaobject PRIVATE -fsanitize=address)

target_link_libraries(cpp26metaobject PRIVATE
    Qt6::CorePrivate
    Qt6::Widgets
    )

if (TARGET Qt6::Quick)
    target_link_libraries(cpp26metaobject PRIVATE
       Qt6::Quick Qt6::QuickControls2
       Qt6::QuickWidgets
    )
endif()

if (TARGET Qt6::Multimedia)
    target_link_libraries(cpp26metaobject PRIVATE
        Qt6::Multimedia
    )
endif()

set_property(TARGET cpp26metaobject APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
             ${CMAKE_CURRENT_SOURCE_DIR})

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "16.0")
    target_compile_options(cpp26metaobject PRIVATE -std=c++26)
    target_compile_options(cpp26metaobject PRIVATE -freflection)
elseif ("${CMAKE_CXX_COMPILE_FEATURES}" MATCHES "cxx_std_23")
    set_property(TARGET cpp26metaobject PROPERTY CXX_STANDARD 23)
endif()

qt_finalize_executable(cpp26metaobject)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
