QT += core gui network widgets svg

TARGET = WerewolfClient
TEMPLATE = app

INCLUDEPATH += headers

SOURCES += sources/main.cpp \
           sources/PlayerAvatarWidget.cpp \
           sources/mainwindow.cpp \
           sources/networkmanager.cpp

HEADERS += headers/mainwindow.h \
           headers/PlayerAvatarWidget.h \
           headers/networkmanager.h \
           headers/protocol.h

# 移动端主题、背景和头像素材统一编译进程序，运行时不依赖网络资源。
RESOURCES += resources/resources.qrc

DISTFILES += resources/styles/mobile_theme.qss \
             resources/images/moon_forest.svg \
             resources/images/avatar_villager.svg \
             resources/images/app_icon_master.png \
             resources/images/app_icon_store_512.png

CONFIG += c++17
CONFIG += warn_on

# Android 使用同一套 Widgets 代码和通信逻辑，界面由响应式布局适配竖屏。
android {
    QMAKE_TARGET_PRODUCT = 月夜议会
    QMAKE_TARGET_DESCRIPTION = 狼人杀移动客户端
    ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android
    ANDROID_VERSION_CODE = 1
    ANDROID_VERSION_NAME = 1.0.0
    ANDROID_ABIS = arm64-v8a
}

DISTFILES += android/AndroidManifest.xml \
             android/gradle/wrapper/gradle-wrapper.properties
