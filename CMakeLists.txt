cmake_minimum_required(VERSION 3.28)
project(cpp-proc-monitor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Enable std::expected in GCC 14 ──
add_compile_definitions(_GLIBCXX_USE_CXX11_ABI=1)

# ── Dependencies (FetchContent) ──
include(FetchContent)

# spdlog (header-only logging)
FetchContent_Declare(spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG        v1.15.1
)
FetchContent_MakeAvailable(spdlog)

# nlohmann_json (header-only JSON)
FetchContent_Declare(nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# FTXUI (terminal UI library)
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG        v5.0.0
  SYSTEM
)
FetchContent_MakeAvailable(ftxui)

# ── Library (everything except main.cpp) ──
file(GLOB_RECURSE LIB_SRCS src/*.cpp)
list(FILTER LIB_SRCS EXCLUDE REGEX "main\\.cpp$")
file(GLOB_RECURSE HEADERS include/*.hpp include/*.h)

add_library(proc-monitor-lib STATIC ${LIB_SRCS} ${HEADERS})
target_include_directories(proc-monitor-lib PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(proc-monitor-lib PUBLIC
  spdlog::spdlog
  nlohmann_json::nlohmann_json
  ftxui::screen
  ftxui::dom
  ftxui::component
  pthread
)

# ── Executable ──
add_executable(proc-monitor src/main.cpp)
target_link_libraries(proc-monitor PRIVATE proc-monitor-lib)

# ── Tests ──
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# ── Install ──
install(TARGETS proc-monitor DESTINATION bin)
