# ---------------------------------------------------------------------------
# host native 单测：tests/unit/**/*.cpp 每个文件一个可执行 + ctest 用例。
# 依赖 googletest（FetchContent；env.sh 可把 FETCHCONTENT_SOURCE_DIR_GOOGLETEST
# 指向本机已有源码缓存，离线复用）。
# ---------------------------------------------------------------------------
include(FetchContent)

if(NOT FETCHCONTENT_SOURCE_DIR_GOOGLETEST)
    set(FETCHCONTENT_QUIET OFF)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
    )
endif()
FetchContent_MakeAvailable(googletest)

file(GLOB_RECURSE MAP_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/*.cpp")

foreach(src ${MAP_TEST_SOURCES})
    get_filename_component(test_name ${src} NAME_WE)
    add_executable(${test_name} ${src})
    target_link_libraries(${test_name} PRIVATE earth_engine_core GTest::gtest_main)
    # 仓库根（真实资产/数据 fixture 测试定位文件用；文件缺失时用例自行 GTEST_SKIP）。
    target_compile_definitions(${test_name} PRIVATE
        MAPC_REPO_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/..")
    add_test(NAME ${test_name} COMMAND ${test_name})
    # HTTP 测试需要 libcurl 与本地回环服务（ctest 串行跑，回环端口冲突风险低）。
    if(MAP_CURL_LIBRARY AND test_name STREQUAL "test_http_bytes_source")
        target_compile_definitions(${test_name} PRIVATE MAP_HTTP_AVAILABLE=1)
    endif()
endforeach()
