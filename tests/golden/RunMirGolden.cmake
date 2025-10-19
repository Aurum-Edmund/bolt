cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED BOLTCC)
  message(FATAL_ERROR "BOLTCC executable path not provided")
endif()

if(NOT DEFINED SOURCE)
  message(FATAL_ERROR "SOURCE file not provided")
endif()

if(NOT DEFINED GOLDEN)
  message(FATAL_ERROR "GOLDEN canonical file not provided")
endif()

if(NOT DEFINED OUTPUT)
  message(FATAL_ERROR "OUTPUT path not provided")
endif()

get_filename_component(_output_dir "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${_output_dir}")

execute_process(
  COMMAND "${BOLTCC}" "${SOURCE}" --no-dump-mir --emit-mir-canonical=${OUTPUT}
  RESULT_VARIABLE _run_result
)
if(NOT _run_result EQUAL 0)
  message(FATAL_ERROR "boltcc failed with code ${_run_result}")
endif()

execute_process(
  COMMAND ${CMAKE_COMMAND} -E compare_files "${OUTPUT}" "${GOLDEN}"
  RESULT_VARIABLE _cmp_result
)
if(NOT _cmp_result EQUAL 0)
  message(FATAL_ERROR "MIR canonical mismatch for ${SOURCE}")
endif()

file(REMOVE "${OUTPUT}")
