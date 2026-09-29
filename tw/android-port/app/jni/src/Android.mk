LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := main

SDL_PATH := ../SDL

LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(SDL_PATH)/include \
    $(LOCAL_PATH) \
    $(LOCAL_PATH)/../curl/include

# Sayri application sources
LOCAL_SRC_FILES := \
    main_android.c \
    ui.c \
    pulsarUI.c \
    background.c \
    panel.c \
    button.c \
    primaryButton.c \
    slider.c \
    radio.c \
    checkbox.c \
    searchBar.c \
    hamburger.c \
    sidebar.c \
    dialogBox.c \
    dropdownmenu.c \
    progressBar.c \
    tabs.c \
    menuBar.c \
    tooltips.c \
    notifications.c \
    image.c \
    imageWidget.c \
    textinput.c \
    orb.c \
    toggle.c \
    popup.c \
    history_android.c \
    downloads.c \
    ollama_android.c \
    ipc_android.c

LOCAL_SHARED_LIBRARIES := SDL2 SDL2_ttf SDL2_image

LOCAL_LDLIBS := -lGLESv1_CM -lGLESv2 -lOpenSLES -llog -landroid \
    $(LOCAL_PATH)/../curl/lib/$(TARGET_ARCH_ABI)/libcurl.a -lssl -lcrypto

include $(BUILD_SHARED_LIBRARY)
