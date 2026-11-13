cmake_minimum_required(VERSION 3.25)
project(gtavxr_runtime_bridge LANGUAGES CXX)

find_path(OPENXR_INCLUDE_DIR NAMES openxr/openxr.h REQUIRED)

add_library(gtavxr_native_openxr SHARED bridge.cpp)
target_compile_features(gtavxr_native_openxr PRIVATE cxx_std_20)
target_compile_options(gtavxr_native_openxr PRIVATE /utf-8 /W4 /EHsc)
target_compile_definitions(gtavxr_native_openxr PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_include_directories(gtavxr_native_openxr PRIVATE "${OPENXR_INCLUDE_DIR}")
target_link_libraries(gtavxr_native_openxr PRIVATE d3d11 dxgi gdi32 user32)

add_library(gtavxr_ngx_forwarder SHARED nr_forwarder.cpp)
set_target_properties(gtavxr_ngx_forwarder PROPERTIES OUTPUT_NAME "nvngx.dll_gtavxr")
target_compile_options(gtavxr_ngx_forwarder PRIVATE /W4 /EHsc)
target_compile_definitions(gtavxr_ngx_forwarder PRIVATE NOMINMAX)
target_link_libraries(gtavxr_ngx_forwarder PRIVATE user32 advapi32)
