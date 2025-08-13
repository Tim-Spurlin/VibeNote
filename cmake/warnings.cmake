# Copyright (c) VibeNote contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Function: vibenote_set_warnings
# Applies consistent compiler warning flags to a given target.
# - Skips targets located under third_party/ to avoid noisy output
# - Supports GCC/Clang and MSVC compilers
# - Enables optional warnings-as-errors through VIBENOTE_STRICT_WARNINGS
# - Suppresses Qt MOC generated unused-parameter warnings

function(vibenote_set_warnings target)
  if(NOT TARGET ${target})
    message(FATAL_ERROR "vibenote_set_warnings: target '${target}' does not exist")
  endif()

  # Skip third_party targets entirely
  get_target_property(_source_dir ${target} SOURCE_DIR)
  if(_source_dir)
    file(RELATIVE_PATH _rel_source "${CMAKE_SOURCE_DIR}" "${_source_dir}")
    if(_rel_source MATCHES "^third_party/")
      return()
    endif()
  endif()

  set(_gcc_clang_warnings
      -Wall
      -Wextra
      -Wpedantic
      -Wconversion
      -Wshadow
      -Wnon-virtual-dtor
      -Wold-style-cast
      -Wcast-align
      -Wunused
      -Woverloaded-virtual
      -Wformat=2
      -Wmisleading-indentation)

  set(_msvc_warnings /W4)

  foreach(_scope PRIVATE INTERFACE)
    target_compile_options(${target} ${_scope}
      $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>>:${_gcc_clang_warnings}>
      $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>>:${_msvc_warnings}>
      $<$<AND:$<BOOL:VIBENOTE_STRICT_WARNINGS>,$<COMPILE_LANGUAGE:CXX>,$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>>:-Werror>
      $<$<AND:$<BOOL:VIBENOTE_STRICT_WARNINGS>,$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>>:/WX>
      $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<BOOL:$<TARGET_PROPERTY:AUTOMOC>>>:-Wno-unused-parameter>
    )
  endforeach()

  # Apply suppression to autogen target when present to silence moc warnings
  if(TARGET ${target}_autogen)
    target_compile_options(${target}_autogen PRIVATE -Wno-unused-parameter)
  endif()
endfunction()

