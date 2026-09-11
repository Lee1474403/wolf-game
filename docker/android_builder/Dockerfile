FROM ubuntu:22.04

ARG DEBIAN_FRONTEND=noninteractive
ARG QT_VERSION=6.10.2
ARG ANDROID_CMDLINE_TOOLS=11076708
ARG ANDROID_API=35
ARG ANDROID_BUILD_TOOLS=35.0.0
ARG ANDROID_NDK=27.2.12479018

# 国内网络可按需在下一条 RUN 前启用镜像源，例如：
# RUN sed -i 's@archive.ubuntu.com@mirrors.aliyun.com@g' /etc/apt/sources.list
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       ca-certificates curl unzip python3 python3-pip openjdk-17-jdk-headless \
       build-essential make git file \
    && rm -rf /var/lib/apt/lists/* \
    && python3 -m pip install --no-cache-dir aqtinstall

ENV ANDROID_SDK_ROOT=/opt/android-sdk \
    ANDROID_HOME=/opt/android-sdk \
    JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64

RUN mkdir -p ${ANDROID_SDK_ROOT}/cmdline-tools \
    && curl -fsSL "https://dl.google.com/android/repository/commandlinetools-linux-${ANDROID_CMDLINE_TOOLS}_latest.zip" -o /tmp/tools.zip \
    && unzip -q /tmp/tools.zip -d /tmp/android-tools \
    && mv /tmp/android-tools/cmdline-tools ${ANDROID_SDK_ROOT}/cmdline-tools/latest \
    && rm -rf /tmp/tools.zip /tmp/android-tools \
    && yes | ${ANDROID_SDK_ROOT}/cmdline-tools/latest/bin/sdkmanager --licenses >/dev/null \
    && ${ANDROID_SDK_ROOT}/cmdline-tools/latest/bin/sdkmanager \
       "platform-tools" "platforms;android-${ANDROID_API}" \
       "build-tools;${ANDROID_BUILD_TOOLS}" "ndk;${ANDROID_NDK}"

RUN python3 -m aqt install-qt linux desktop "${QT_VERSION}" linux_gcc_64 -O /opt/Qt \
    && python3 -m aqt install-qt linux android "${QT_VERSION}" android_arm64_v8a -O /opt/Qt

ENV QT_HOST_PATH=/opt/Qt/${QT_VERSION}/gcc_64 \
    QT_ANDROID_PATH=/opt/Qt/${QT_VERSION}/android_arm64_v8a \
    ANDROID_NDK_ROOT=/opt/android-sdk/ndk/${ANDROID_NDK}
ENV PATH=${QT_ANDROID_PATH}/bin:${QT_HOST_PATH}/bin:${ANDROID_SDK_ROOT}/platform-tools:${ANDROID_SDK_ROOT}/cmdline-tools/latest/bin:${ANDROID_SDK_ROOT}/build-tools/${ANDROID_BUILD_TOOLS}:${PATH}

COPY docker/android_builder/build_android.sh /usr/local/bin/build_android.sh
RUN chmod +x /usr/local/bin/build_android.sh

WORKDIR /workspace
ENTRYPOINT ["/usr/local/bin/build_android.sh"]
