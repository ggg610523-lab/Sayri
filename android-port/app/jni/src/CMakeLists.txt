cmake_minimum_required(VERSION 3.22)

project(Sayri_App)

# Our application compiled into libmain.so.
# SDL2_image provides the SDL2_image target; SDL_ttf provides
# SDL2_ttf; SDL2 provides SDL2 / SDL2main. Linking against these
# CMake targets pulls in their transitive dependencies.

# libcurl comes from a prebuilt Android tree placed in
# jni/curl (see the README). Expected layout:
#   curl/include/curl/curl.h
#   curl/lib/${ANDROID_ABI}/libcurl.a   (or libcurl.so)
set(CURL_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../curl)

set(APP_SOURCES
    main_android.c
    ui.c
    pulsarUI.c
    background.c
    panel.c
    button.c
    primaryButton.c
    slider.c
    radio.c
    checkbox.c
    searchBar.c
    hamburger.c
    sidebar.c
    dialogBox.c
    dropdownmenu.c
    progressBar.c
    tabs.c
    menuBar.c
    tooltips.c
    notifications.c
    image.c
    imageWidget.c
    textinput.c
    orb.c
    toggle.c
    popup.c
    history_android.c
    downloads.c
    ollama_android.c
    ipc_android.c
)

add_library(main SHARED ${APP_SOURCES})

target_include_directories(main PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/compat_include
    ${CURL_ROOT}/include
)

target_link_libraries(main
    SDL2
    SDL2_image
    SDL2_ttf
)

# Link libcurl (prebuilt for the target ABI). libcurl.a pulls in zlib
# for gzip/Content-Encoding decoding; link against Android's zlib.
if(EXISTS ${CURL_ROOT}/lib/${ANDROID_ABI}/libcurl.a)
    target_link_libraries(main ${CURL_ROOT}/lib/${ANDROID_ABI}/libcurl.a z)
elseif(EXISTS ${CURL_ROOT}/lib/${ANDROID_ABI}/libcurl.so)
    target_link_libraries(main ${CURL_ROOT}/lib/${ANDROID_ABI}/libcurl.so)
else()
    message(FATAL_ERROR "libcurl not found for ABI ${ANDROID_ABI}. "
        "Place it at ${CURL_ROOT}/lib/${ANDROID_ABI}/ (see README).")
endif()
