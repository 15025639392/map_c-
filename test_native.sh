#!/usr/bin/env bash
# host native 单测入口（最短反馈回路）。用法：
#   ./test_native.sh                 # 全量
#   ./test_native.sh test_ellipsoid  # 跑单个测试
set -euo pipefail
cd "$(dirname "$0")"
source env.sh

cmake --preset native-tests >/dev/null
cmake --build --preset native-tests

if [ "$#" -gt 0 ]; then
    # 名称过滤：直接透传给 ctest
    ctest --preset native-tests -R "$1" --output-on-failure
else
    ctest --preset native-tests --output-on-failure
fi
