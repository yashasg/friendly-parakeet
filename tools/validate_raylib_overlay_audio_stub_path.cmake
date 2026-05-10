set(_repo_root "${CMAKE_CURRENT_LIST_DIR}/..")
include("${_repo_root}/vcpkg-overlay/raylib/audio_stub_helpers.cmake")

set(_scratch "${_repo_root}/build/ci/raylib-overlay-audio-stub-check")
set(_header "dr_flac.h")
set(_expected_path "${_scratch}/src/external/${_header}")

file(REMOVE_RECURSE "${_scratch}")
file(MAKE_DIRECTORY "${_scratch}/src/external")

raylib_write_audio_disabled_stub("${_scratch}" "${_header}")

if(NOT EXISTS "${_expected_path}")
    message(FATAL_ERROR "audio-disabled stub was not written to expected path: ${_expected_path}")
endif()

file(READ "${_expected_path}" _contents)
if(NOT _contents STREQUAL "# audio disabled")
    message(FATAL_ERROR "audio-disabled stub payload mismatch: '${_contents}'")
endif()

message(STATUS "raylib overlay audio-disabled stub path validation passed")
