# syntax=docker/dockerfile:1.6

ARG MOD_ID
ARG TARGETS=android32,android64,ios,macos,windows
ARG CPM_CACHE_DIR=/workspace/cpm-cache
ARG BINDINGS=geode-sdk/bindings

# Android 32
FROM prevter/geode-sdk:android-latest AS android32
ARG MOD_ID
ARG CPM_CACHE_DIR
ARG TARGETS

RUN if ! echo "$TARGETS" | grep -q 'android32'; then exit 0; fi

WORKDIR /workspace
RUN git clone https://github.com/${BINDINGS} --depth=1

WORKDIR /workspace/project
COPY . .

RUN --mount=type=cache,target=/workspace/cpm-cache \
    cmake -G Ninja -B build \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCPM_SOURCE_CACHE=${CPM_CACHE_DIR} \
      -DGEODE_BINDINGS_REPO_PATH=/workspace/bindings \
      -DCMAKE_TOOLCHAIN_FILE=/opt/android-ndk/build/cmake/android.toolchain.cmake \
      -DANDROID_ABI=armeabi-v7a \
      -DANDROID_PLATFORM=android-23 \
      -DANDROID_STL=c++_shared
RUN --mount=type=cache,target=/workspace/cpm-cache \
    cmake --build build --parallel

# Android 64
FROM prevter/geode-sdk:android-latest AS android64
ARG MOD_ID
ARG CPM_CACHE_DIR
ARG TARGETS

RUN if ! echo "$TARGETS" | grep -q 'android64'; then exit 0; fi

WORKDIR /workspace
RUN git clone https://github.com/${BINDINGS} --depth=1

WORKDIR /workspace/project
COPY . .

RUN --mount=type=cache,target=/workspace/cpm-cache \
    cmake -G Ninja -B build \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCPM_SOURCE_CACHE=${CPM_CACHE_DIR} \
      -DGEODE_BINDINGS_REPO_PATH=/workspace/bindings \
      -DCMAKE_TOOLCHAIN_FILE=/opt/android-ndk/build/cmake/android.toolchain.cmake \
      -DANDROID_ABI=arm64-v8a \
      -DANDROID_PLATFORM=android-23 \
      -DANDROID_STL=c++_shared
RUN --mount=type=cache,target=/workspace/cpm-cache \
    cmake --build build --parallel

# iOS
FROM prevter/geode-sdk:ios-latest AS ios
ARG MOD_ID
ARG CPM_CACHE_DIR
ARG TARGETS

RUN if ! echo "$TARGETS" | grep -q 'ios'; then exit 0; fi

WORKDIR /workspace
RUN git clone https://github.com/${BINDINGS} --depth=1
RUN geode sdk update nightly

WORKDIR /workspace/project
COPY . .

RUN --mount=type=cache,target=/workspace/cpm-cache \
    cmake -G Ninja -B build \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCPM_SOURCE_CACHE=${CPM_CACHE_DIR} \
      -DGEODE_BINDINGS_REPO_PATH=/workspace/bindings \
      -DGEODE_IOS_SDK="${CMAKE_OSX_SYSROOT}"
RUN --mount=type=cache,target=/workspace/cpm-cache \
    cmake --build build --parallel

# MacOS
FROM prevter/geode-sdk:macos-latest AS macos
ARG MOD_ID
ARG CPM_CACHE_DIR
ARG TARGETS

RUN if ! echo "$TARGETS" | grep -q 'macos'; then exit 0; fi

WORKDIR /workspace
RUN git clone https://github.com/${BINDINGS} --depth=1

WORKDIR /workspace/project
COPY . .

RUN --mount=type=cache,target=/workspace/cpm-cache \
    cmake -G Ninja -B build \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCPM_SOURCE_CACHE=${CPM_CACHE_DIR} \
      -DGEODE_BINDINGS_REPO_PATH=/workspace/bindings
RUN --mount=type=cache,target=/workspace/cpm-cache \
    cmake --build build --parallel

# Windows
FROM prevter/geode-sdk:windows-latest AS windows
ARG MOD_ID
ARG CPM_CACHE_DIR
ARG TARGETS

RUN if ! echo "$TARGETS" | grep -q 'windows'; then exit 0; fi

WORKDIR /workspace
RUN git clone https://github.com/${BINDINGS} --depth=1

WORKDIR /workspace/project
COPY . .

RUN --mount=type=cache,target=/workspace/cpm-cache \
    cmake -G Ninja -B build \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCPM_SOURCE_CACHE=${CPM_CACHE_DIR} \
      -DGEODE_BINDINGS_REPO_PATH=/workspace/bindings \
      -DCMAKE_TOOLCHAIN_FILE=/root/.local/share/Geode/cross-tools/clang-msvc-sdk/clang-msvc.cmake \
      -DSPLAT_DIR=/root/.local/share/Geode/cross-tools/splat \
      -DHOST_ARCH=x64
RUN --mount=type=cache,target=/workspace/cpm-cache \
    cmake --build build --parallel

# Combine
FROM prevter/geode-sdk:windows-latest AS combine
ARG MOD_ID

WORKDIR /workspace

COPY --from=android32 /workspace/project/build/${MOD_ID}.geode ./android32/${MOD_ID}.geode
COPY --from=android64 /workspace/project/build/${MOD_ID}.geode ./android64/${MOD_ID}.geode
COPY --from=ios       /workspace/project/build/${MOD_ID}.geode ./ios/${MOD_ID}.geode
COPY --from=macos     /workspace/project/build/${MOD_ID}.geode ./macos/${MOD_ID}.geode
COPY --from=windows   /workspace/project/build/${MOD_ID}.geode ./windows/${MOD_ID}.geode

RUN geode package merge \
    ./android32/${MOD_ID}.geode \
    ./android64/${MOD_ID}.geode \
    ./ios/${MOD_ID}.geode \
    ./macos/${MOD_ID}.geode \
    ./windows/${MOD_ID}.geode

RUN mkdir -p /output &&  cp ./android32/${MOD_ID}.geode /output/${MOD_ID}.geode

FROM scratch AS combine-export
ARG MOD_ID

COPY --from=combine /output/${MOD_ID}.geode /
