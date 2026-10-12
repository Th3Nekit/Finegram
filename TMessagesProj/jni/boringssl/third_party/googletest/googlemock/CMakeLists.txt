option(gmock_build_tests "Build all of Google Mock's own tests." OFF)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/gtest/CMakeLists.txt")
  set(gtest_dir gtest)
else()
  set(gtest_dir ../googletest)
endif()

include("${gtest_dir}/cmake/hermetic_build.cmake" OPTIONAL)

if (COMMAND pre_project_set_up_hermetic_build)

  pre_project_set_up_hermetic_build()
endif()

cmake_minimum_required(VERSION 3.13)
project(gmock VERSION ${GOOGLETEST_VERSION} LANGUAGES CXX C)

if (COMMAND set_up_hermetic_build)
  set_up_hermetic_build()
endif()

add_subdirectory("${gtest_dir}" "${gmock_BINARY_DIR}/${gtest_dir}")

if(CMAKE_PROJECT_NAME STREQUAL "gmock" OR CMAKE_PROJECT_NAME STREQUAL "googletest-distribution")

  option(BUILD_SHARED_LIBS "Build shared libraries (DLLs)." OFF)
else()
  mark_as_advanced(gmock_build_tests)
endif()

config_compiler_and_linker()

get_target_property(gtest_include_dirs gtest INCLUDE_DIRECTORIES)
set(gmock_build_include_dirs
  "${gmock_SOURCE_DIR}/include"
  "${gmock_SOURCE_DIR}"
  "${gtest_include_dirs}")
include_directories(${gmock_build_include_dirs})

if (MSVC)
  cxx_library(gmock
              "${cxx_strict}"
              "${gtest_dir}/src/gtest-all.cc"
              src/gmock-all.cc)

  cxx_library(gmock_main
              "${cxx_strict}"
              "${gtest_dir}/src/gtest-all.cc"
              src/gmock-all.cc
              src/gmock_main.cc)
else()
  cxx_library(gmock "${cxx_strict}" src/gmock-all.cc)
  target_link_libraries(gmock PUBLIC gtest)
  set_target_properties(gmock PROPERTIES VERSION ${GOOGLETEST_VERSION})
  cxx_library(gmock_main "${cxx_strict}" src/gmock_main.cc)
  target_link_libraries(gmock_main PUBLIC gmock)
  set_target_properties(gmock_main PROPERTIES VERSION ${GOOGLETEST_VERSION})
endif()

string(REPLACE ";" "$<SEMICOLON>" dirs "${gmock_build_include_dirs}")
target_include_directories(gmock SYSTEM INTERFACE
  "$<BUILD_INTERFACE:${dirs}>"
  "$<INSTALL_INTERFACE:$<INSTALL_PREFIX>/${CMAKE_INSTALL_INCLUDEDIR}>")
target_include_directories(gmock_main SYSTEM INTERFACE
  "$<BUILD_INTERFACE:${dirs}>"
  "$<INSTALL_INTERFACE:$<INSTALL_PREFIX>/${CMAKE_INSTALL_INCLUDEDIR}>")

install_project(gmock gmock_main)

if (gmock_build_tests)

  enable_testing()

  if (MINGW OR CYGWIN)
    add_compile_options("-Wa,-mbig-obj")
  endif()

  cxx_test(gmock-actions_test gmock_main)
  cxx_test(gmock-cardinalities_test gmock_main)
  cxx_test(gmock_ex_test gmock_main)
  cxx_test(gmock-function-mocker_test gmock_main)
  cxx_test(gmock-internal-utils_test gmock_main)
  cxx_test(gmock-matchers-arithmetic_test gmock_main)
  cxx_test(gmock-matchers-comparisons_test gmock_main)
  cxx_test(gmock-matchers-containers_test gmock_main)
  cxx_test(gmock-matchers-misc_test gmock_main)
  cxx_test(gmock-more-actions_test gmock_main)
  cxx_test(gmock-nice-strict_test gmock_main)
  cxx_test(gmock-port_test gmock_main)
  cxx_test(gmock-spec-builders_test gmock_main)
  cxx_test(gmock_link_test gmock_main test/gmock_link2_test.cc)
  cxx_test(gmock_test gmock_main)

  if (DEFINED GTEST_HAS_PTHREAD)
    cxx_test(gmock_stress_test gmock)
  endif()

  if (MSVC)
    cxx_library(gmock_main_no_exception "${cxx_no_exception}"
      "${gtest_dir}/src/gtest-all.cc" src/gmock-all.cc src/gmock_main.cc)

    cxx_library(gmock_main_no_rtti "${cxx_no_rtti}"
      "${gtest_dir}/src/gtest-all.cc" src/gmock-all.cc src/gmock_main.cc)

  else()
    cxx_library(gmock_main_no_exception "${cxx_no_exception}" src/gmock_main.cc)
    target_link_libraries(gmock_main_no_exception PUBLIC gmock)

    cxx_library(gmock_main_no_rtti "${cxx_no_rtti}" src/gmock_main.cc)
    target_link_libraries(gmock_main_no_rtti PUBLIC gmock)
  endif()
  cxx_test_with_flags(gmock-more-actions_no_exception_test "${cxx_no_exception}"
    gmock_main_no_exception test/gmock-more-actions_test.cc)

  cxx_test_with_flags(gmock_no_rtti_test "${cxx_no_rtti}"
    gmock_main_no_rtti test/gmock-spec-builders_test.cc)

  cxx_shared_library(shared_gmock_main "${cxx_default}"
    "${gtest_dir}/src/gtest-all.cc" src/gmock-all.cc src/gmock_main.cc)

  cxx_executable_with_flags(shared_gmock_test_ "${cxx_default}"
    shared_gmock_main test/gmock-spec-builders_test.cc)
  set_target_properties(shared_gmock_test_
    PROPERTIES
    COMPILE_DEFINITIONS "GTEST_LINKED_AS_SHARED_LIBRARY=1")

  cxx_executable(gmock_leak_test_ test gmock_main)
  py_test(gmock_leak_test)

  cxx_executable(gmock_output_test_ test gmock)
  py_test(gmock_output_test)
endif()
