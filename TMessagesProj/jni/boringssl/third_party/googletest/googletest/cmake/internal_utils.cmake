macro(fix_default_compiler_settings_)
  if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC|Clang")

    foreach (flag_var
             CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
             CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO
             CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
             CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
      if (NOT BUILD_SHARED_LIBS AND NOT gtest_force_shared_crt)

        string(REPLACE "/MD" "-MT" ${flag_var} "${${flag_var}}")

        string(REPLACE "-D_DLL" "" ${flag_var} "${${flag_var}}")
      endif()

      string(REPLACE "/W3" "/W4" ${flag_var} "${${flag_var}}")

      string(REPLACE "/EHsc" "" ${flag_var} "${${flag_var}}")
    endforeach()
  endif()
endmacro()

macro(config_compiler_and_linker)

  unset(GTEST_HAS_PTHREAD)
  if (NOT gtest_disable_pthreads AND NOT MINGW)

    find_package(Threads)
    if (CMAKE_USE_PTHREADS_INIT)
      set(GTEST_HAS_PTHREAD ON)
    endif()
  endif()

  fix_default_compiler_settings_()
  if (MSVC)

    set(cxx_base_flags "-GS -W4 -WX -wd4251 -wd4275 -nologo -J")
    set(cxx_base_flags "${cxx_base_flags} -D_UNICODE -DUNICODE -DWIN32 -D_WIN32")
    set(cxx_base_flags "${cxx_base_flags} -DSTRICT -DWIN32_LEAN_AND_MEAN")
    set(cxx_exception_flags "-EHsc -D_HAS_EXCEPTIONS=1")
    set(cxx_no_exception_flags "-EHs-c- -D_HAS_EXCEPTIONS=0")
    set(cxx_no_rtti_flags "-GR-")

    set(cxx_base_flags "${cxx_base_flags} -wd4702")

    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
      set(cxx_base_flags "${cxx_base_flags} -utf-8")
    endif()
    if (CMAKE_CXX_COMPILER_ID STREQUAL "IntelLLVM")
      set(cxx_base_flags "${cxx_base_flags} /fp:precise -Wno-inconsistent-missing-override -Wno-microsoft-exception-spec -Wno-unused-function -Wno-unused-but-set-variable")
    endif()
  elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR
      CMAKE_CXX_COMPILER_ID STREQUAL "IntelLLVM")
    set(cxx_base_flags "-Wall -Wshadow -Wconversion -Wundef")
    set(cxx_exception_flags "-fexceptions")
    set(cxx_no_exception_flags "-fno-exceptions")
    set(cxx_strict_flags "-W -Wpointer-arith -Wreturn-type -Wcast-qual -Wwrite-strings -Wswitch -Wunused-parameter -Wcast-align -Winline -Wredundant-decls")
    set(cxx_no_rtti_flags "-fno-rtti")
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
      set(cxx_strict_flags "${cxx_strict_flags} -Wchar-subscripts")
    endif()
    if (CMAKE_CXX_COMPILER_ID STREQUAL "IntelLLVM")
      set(cxx_base_flags "${cxx_base_flags} -Wno-implicit-float-size-conversion -ffp-model=precise")
    endif()
  elseif (CMAKE_COMPILER_IS_GNUCXX)
    set(cxx_base_flags "-Wall -Wshadow -Wundef")
    if(NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7.0.0)
      set(cxx_base_flags "${cxx_base_flags} -Wno-error=dangling-else")
    endif()
    set(cxx_exception_flags "-fexceptions")
    set(cxx_no_exception_flags "-fno-exceptions")

    set(cxx_no_rtti_flags "-fno-rtti -DGTEST_HAS_RTTI=0")
    set(cxx_strict_flags
      "-Wextra -Wno-unused-parameter -Wno-missing-field-initializers")
  elseif (CMAKE_CXX_COMPILER_ID STREQUAL "SunPro")
    set(cxx_exception_flags "-features=except")

    set(cxx_no_exception_flags "-features=no%except -DGTEST_HAS_EXCEPTIONS=0")
    set(cxx_no_rtti_flags "-features=no%rtti -DGTEST_HAS_RTTI=0")
  elseif (CMAKE_CXX_COMPILER_ID STREQUAL "VisualAge" OR
      CMAKE_CXX_COMPILER_ID STREQUAL "XL")

    set(cxx_exception_flags "-qeh")
    set(cxx_no_exception_flags "-qnoeh")

    set(cxx_no_rtti_flags "-qnortti -DGTEST_HAS_RTTI=0")
  elseif (CMAKE_CXX_COMPILER_ID STREQUAL "HP")
    set(cxx_base_flags "-AA -mt")
    set(cxx_exception_flags "-DGTEST_HAS_EXCEPTIONS=1")
    set(cxx_no_exception_flags "+noeh -DGTEST_HAS_EXCEPTIONS=0")

    set(cxx_no_rtti_flags "")
  endif()

  if (DEFINED GTEST_HAS_PTHREAD)
    set(GTEST_HAS_PTHREAD_MACRO "-DGTEST_HAS_PTHREAD=1")
  else()
    set(GTEST_HAS_PTHREAD_MACRO "-DGTEST_HAS_PTHREAD=0")
  endif()
  set(cxx_base_flags "${cxx_base_flags} ${GTEST_HAS_PTHREAD_MACRO}")

  set(cxx_exception "${cxx_base_flags} ${cxx_exception_flags}")
  set(cxx_no_exception
    "${CMAKE_CXX_FLAGS} ${cxx_base_flags} ${cxx_no_exception_flags}")
  set(cxx_default "${cxx_exception}")
  set(cxx_no_rtti "${cxx_default} ${cxx_no_rtti_flags}")

  set(cxx_strict "${cxx_default} ${cxx_strict_flags}")
endmacro()

function(cxx_library_with_type name type cxx_flags)

  add_library(${name} ${type} ${ARGN})
  add_library(${cmake_package_name}::${name} ALIAS ${name})
  set_target_properties(${name}
    PROPERTIES
    COMPILE_FLAGS "${cxx_flags}")

  set_target_properties(${name}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

  get_target_property(pdb_debug_postfix ${name} DEBUG_POSTFIX)
  set_target_properties(${name}
    PROPERTIES
    PDB_NAME "${name}"
    PDB_NAME_DEBUG "${name}${pdb_debug_postfix}"
    COMPILE_PDB_NAME "${name}"
    COMPILE_PDB_NAME_DEBUG "${name}${pdb_debug_postfix}")

  if (BUILD_SHARED_LIBS OR type STREQUAL "SHARED")
    set_target_properties(${name}
      PROPERTIES
      COMPILE_DEFINITIONS "GTEST_CREATE_SHARED_LIBRARY=1")
    target_compile_definitions(${name} INTERFACE
      $<INSTALL_INTERFACE:GTEST_LINKED_AS_SHARED_LIBRARY=1>)
  endif()
  if (DEFINED GTEST_HAS_PTHREAD)
    target_link_libraries(${name} PUBLIC Threads::Threads)
  endif()

  target_compile_features(${name} PUBLIC cxx_std_17)
endfunction()

function(cxx_shared_library name cxx_flags)
  cxx_library_with_type(${name} SHARED "${cxx_flags}" ${ARGN})
endfunction()

function(cxx_library name cxx_flags)
  cxx_library_with_type(${name} "" "${cxx_flags}" ${ARGN})
endfunction()

function(cxx_executable_with_flags name cxx_flags libs)
  add_executable(${name} ${ARGN})
  if (MSVC)

    set(cxx_flags "${cxx_flags} -bigobj")
  endif()
  if (cxx_flags)
    set_target_properties(${name}
      PROPERTIES
      COMPILE_FLAGS "${cxx_flags}")
  endif()
  if (BUILD_SHARED_LIBS)
    set_target_properties(${name}
      PROPERTIES
      COMPILE_DEFINITIONS "GTEST_LINKED_AS_SHARED_LIBRARY=1")
  endif()

  foreach (lib "${libs}")
    target_link_libraries(${name} ${lib})
  endforeach()
endfunction()

function(cxx_executable name dir libs)
  cxx_executable_with_flags(
    ${name} "${cxx_default}" "${libs}" "${dir}/${name}.cc" ${ARGN})
endfunction()

if(gtest_build_tests)
  find_package(Python3)
endif()

function(cxx_test_with_flags name cxx_flags libs)
  cxx_executable_with_flags(${name} "${cxx_flags}" "${libs}" ${ARGN})
    add_test(NAME ${name} COMMAND "$<TARGET_FILE:${name}>")
endfunction()

function(cxx_test name libs)
  cxx_test_with_flags("${name}" "${cxx_default}" "${libs}"
    "test/${name}.cc" ${ARGN})
endfunction()

function(py_test name)
  if (NOT Python3_Interpreter_FOUND)
    return()
  endif()

  get_cmake_property(is_multi "GENERATOR_IS_MULTI_CONFIG")
  set(build_dir "${CMAKE_CURRENT_BINARY_DIR}")
  if (is_multi)
    set(build_dir "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>")
  endif()

  add_test(NAME ${name}
      COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/test/${name}.py
          --build_dir=${build_dir} ${ARGN})

  set_tests_properties(${name} PROPERTIES ENVIRONMENT PYTHONPATH=${CMAKE_SOURCE_DIR})
endfunction()

function(install_project)
  if(INSTALL_GTEST)
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/"
      COMPONENT "${PROJECT_NAME}"
      DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

    install(TARGETS ${ARGN}
      EXPORT ${targets_export_name}
      COMPONENT "${PROJECT_NAME}"
      RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
      ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
      LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
    if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")

      foreach(t ${ARGN})
        get_target_property(t_pdb_name ${t} COMPILE_PDB_NAME)
        get_target_property(t_pdb_name_debug ${t} COMPILE_PDB_NAME_DEBUG)
        get_target_property(t_pdb_output_directory ${t} PDB_OUTPUT_DIRECTORY)
        install(FILES
          "${t_pdb_output_directory}/\${CMAKE_INSTALL_CONFIG_NAME}/$<$<CONFIG:Debug>:${t_pdb_name_debug}>$<$<NOT:$<CONFIG:Debug>>:${t_pdb_name}>.pdb"
          COMPONENT "${PROJECT_NAME}"
          DESTINATION ${CMAKE_INSTALL_LIBDIR}
          OPTIONAL)
      endforeach()
    endif()

    foreach(t ${ARGN})
      set(configured_pc "${generated_dir}/${t}.pc")
      configure_file("${PROJECT_SOURCE_DIR}/cmake/${t}.pc.in"
        "${configured_pc}" @ONLY)
      install(FILES "${configured_pc}"
        COMPONENT "${PROJECT_NAME}"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
    endforeach()
  endif()
endfunction()
