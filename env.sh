#!/usr/bin/env bash

# map_cplus 本地构建环境（host native）。
# 用法：source env.sh   （或直接 ./test_native.sh，脚本内部会 source 本文件）
#
# 本文件只负责"找到本机已有的构建工具"，不下载任何东西：
#   1. cmake / ninja：优先 Android SDK 自带（本机已装 3.22.1）；
#      否则退回 PATH。
#   2. googletest：优先复用 gis-md 构建目录里已 FetchContent 下来的源码缓存
#      （离线可用，不写 gis-md，只读）；没有则走网络 GitHub 拉取。
#   3. 将来需要 glm / nlohmann-json / curl / stb 等依赖时，同样先找本机
#      gis-md vcpkg installed 目录（只读复用），再退回 vcpkg/FetchContent。

_map_cplus_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# --- cmake / ninja ----------------------------------------------------------
if [ -z "${CMAKE_BIN_DIR:-}" ]; then
    for _cand in \
        "$HOME/Library/Android/sdk/cmake/3.22.1/bin" \
        "/usr/local/bin" "/opt/homebrew/bin"; do
        if [ -x "$_cand/cmake" ] && [ -x "$_cand/ninja" ]; then
            CMAKE_BIN_DIR="$_cand"
            break
        fi
    done
fi
if [ -n "${CMAKE_BIN_DIR:-}" ] && [ -d "$CMAKE_BIN_DIR" ]; then
    case ":$PATH:" in
        *":$CMAKE_BIN_DIR:"*) ;;
        *) export PATH="$CMAKE_BIN_DIR:$PATH" ;;
    esac
fi

# --- googletest 源码缓存（FetchContent 离线复用）----------------------------
if [ -z "${FETCHCONTENT_SOURCE_DIR_GOOGLETEST:-}" ]; then
    for _cand in \
        "${GIS_MD_SCAFFOLD_DIR:-/Users/yan/Desktop/work/gis-md/scaffold}/build/native-tests/_deps/googletest-src" \
        "$_map_cplus_root/build/native-tests/_deps/googletest-src"; do
        if [ -f "$_cand/CMakeLists.txt" ]; then
            export FETCHCONTENT_SOURCE_DIR_GOOGLETEST="$_cand"
            break
        fi
    done
fi

# --- glm / nlohmann-json 等（阶段 2+ 用到时启用）----------------------------
if [ -z "${GIS_MD_VCPKG_INSTALLED:-}" ] && \
   [ -d "/Users/yan/Desktop/work/gis-md/scaffold/third_party/vcpkg/installed/arm64-osx" ]; then
    export GIS_MD_VCPKG_INSTALLED="/Users/yan/Desktop/work/gis-md/scaffold/third_party/vcpkg/installed/arm64-osx"
fi

if [ -n "${CMAKE_BIN_DIR:-}" ]; then
    echo "map_cplus env: cmake=$(command -v cmake) ninja=$(command -v ninja)"
    echo "map_cplus env: FETCHCONTENT_SOURCE_DIR_GOOGLETEST=${FETCHCONTENT_SOURCE_DIR_GOOGLETEST:-<未找到，将走网络>}"
fi
