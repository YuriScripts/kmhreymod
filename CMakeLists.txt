cmake_minimum_required(VERSION 3.18.1)
project("kmhrey_menu")

add_library(kmhrey_menu SHARED
    main.cpp
    And64InlineHook.cpp
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_opengl3.cpp
    imgui/backends/imgui_impl_android.cpp
)

# THIS IS THE FIX: Tell CMake where to find the header files
target_include_directories(kmhrey_menu PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends
)

find_library(log-lib log)
find_library(android-lib android)
find_library(EGL-lib EGL)
find_library(GLESv3-lib GLESv3)

target_link_libraries(kmhrey_menu
    ${log-lib}
    ${android-lib}
    ${EGL-lib}
    ${GLESv3-lib}
)
