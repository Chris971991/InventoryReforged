cmake_minimum_required(VERSION 3.21)

project(InventoryReforged VERSION 1.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB sources CONFIGURE_DEPENDS "source/*.cpp" "source/*.h")
# what Know your Skyrim compiles too: who holds what, where a place is, what a person takes from its templates
list(APPEND sources ../shared/source/Holders.cpp ../shared/source/People.cpp ../shared/source/Places.cpp)

# The debug edition logs every step of the plugin and the panel, built into a folder of its own:
#   build.ps1 equipment-panel build/debug IR_DEBUG_EDITION=ON
option(IR_DEBUG_EDITION "build the debug edition" OFF)
if(IR_DEBUG_EDITION)
	file(GLOB debugSources CONFIGURE_DEPENDS "source/Debug/*.cpp" "source/Debug/*.h")
	list(APPEND sources ${debugSources})
endif()

find_package(CommonLibSSE CONFIG REQUIRED)
find_package(spdlog REQUIRED CONFIG)
find_package(directxtk CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_commonlibsse_plugin(${PROJECT_NAME} SOURCES ${sources})
target_precompile_headers(${PROJECT_NAME} PRIVATE source/PCH.h)
# include holds SKSEMenuFramework.h, the header SKSE Menu Framework gives the mods that draw pages in it
target_include_directories(${PROJECT_NAME} PRIVATE source include ../shared/source)
target_link_libraries(${PROJECT_NAME} PRIVATE CommonLibSSE::CommonLibSSE spdlog::spdlog_header_only nlohmann_json::nlohmann_json)
if(IR_DEBUG_EDITION)
	target_compile_definitions(${PROJECT_NAME} PRIVATE IR_DEBUG_EDITION=1)
endif()
