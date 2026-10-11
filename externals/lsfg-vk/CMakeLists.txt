cmake_minimum_required(VERSION 3.16)
set(VOLK_PULL_IN_VULKAN OFF)
set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_ANDROID_KHR)
add_subdirectory(thirdparty/volk EXCLUDE_FROM_ALL)
target_link_libraries(volk PUBLIC vulkan-headers)
set_target_properties(volk PROPERTIES C_VISIBILITY_PRESET hidden POSITION_INDEPENDENT_CODE ON)
add_subdirectory(thirdparty/dxbc EXCLUDE_FROM_ALL)
add_subdirectory(thirdparty/pe-parse/pe-parser-library EXCLUDE_FROM_ALL)
set_target_properties(pe-parse PROPERTIES POSITION_INDEPENDENT_CODE ON)
file(GLOB LSFG_SOURCES CONFIGURE_DEPENDS
    framegen/src/common/*.cpp framegen/src/core/*.cpp framegen/src/pool/*.cpp
    framegen/v3.1p_src/*.cpp framegen/v3.1p_src/shaders/*.cpp)
add_library(azahar_lsfg STATIC ${LSFG_SOURCES} src/extract/trans.cpp)
target_compile_features(azahar_lsfg PRIVATE cxx_std_20)
target_include_directories(azahar_lsfg PUBLIC framegen/public PRIVATE
    framegen/include framegen/v3.1p_include include)
target_link_libraries(azahar_lsfg PRIVATE volk dxbc PUBLIC pe-parse)
set_target_properties(azahar_lsfg PROPERTIES CXX_VISIBILITY_PRESET hidden
    POSITION_INDEPENDENT_CODE ON)
