cmake_minimum_required(VERSION 3.18.1)
project("kmhrey_menu")

add_library(kmhrey_menu SHARED
    main.cpp
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_opengl3.cpp
    imgui/backends/imgui_impl_android.cpp
    dobby/dobby.cc
)

target_link_libraries(kmhrey_menu
    android
    log
    EGL
    GLESv3
)

