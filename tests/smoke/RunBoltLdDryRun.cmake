if (NOT DEFINED BOLT_LD)
    message(FATAL_ERROR "BOLT_LD not specified")
endif ()

if (NOT DEFINED RUNTIME_ROOT)
    message(FATAL_ERROR "RUNTIME_ROOT not specified")
endif ()

if (NOT DEFINED LINKER_SCRIPT)
    message(FATAL_ERROR "LINKER_SCRIPT not specified")
endif ()

if (NOT DEFINED OUTPUT_PATH)
    message(FATAL_ERROR "OUTPUT_PATH not specified")
endif ()

if (NOT DEFINED OBJECT_PATH)
    message(FATAL_ERROR "OBJECT_PATH not specified")
endif ()

if (NOT DEFINED STUB_LINKER)
    message(FATAL_ERROR "STUB_LINKER not specified")
endif ()

if (NOT DEFINED MAP_PATH)
    message(FATAL_ERROR "MAP_PATH not specified")
endif ()

get_filename_component(_smoke_dir "${OUTPUT_PATH}" DIRECTORY)
file(MAKE_DIRECTORY "${_smoke_dir}")

file(WRITE "${OBJECT_PATH}" "")

file(WRITE "${STUB_LINKER}" "#!/bin/sh\nexit 0\n")
file(CHMOD "${STUB_LINKER}"
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

execute_process(
    COMMAND "${BOLT_LD}"
        --emit=air
        --dry-run
        --verbose
        --target=x86_64-air-bolt
        --runtime-root=${RUNTIME_ROOT}
        --linker=${STUB_LINKER}
        --linker-script=${LINKER_SCRIPT}
        --map=${MAP_PATH}
        -o ${OUTPUT_PATH}
        ${OBJECT_PATH}
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
    RESULT_VARIABLE _result
)

if (NOT _result EQUAL 0)
    message(STATUS "bolt-ld stdout:\n${_stdout}")
    message(STATUS "bolt-ld stderr:\n${_stderr}")
    message(FATAL_ERROR "bolt-ld dry run failed with exit code ${_result}")
endif ()

string(FIND "${_stdout}" "dry run: platform linker invocation skipped" _dry_run_index)
if (_dry_run_index EQUAL -1)
    message(STATUS "bolt-ld stdout:\n${_stdout}")
    message(FATAL_ERROR "bolt-ld dry run did not report skipped invocation")
endif ()

string(FIND "${_stdout}" "[bolt-ld] runtime root" _runtime_index)
if (_runtime_index EQUAL -1)
    message(STATUS "bolt-ld stdout:\n${_stdout}")
    message(FATAL_ERROR "bolt-ld dry run did not report runtime root")
endif ()
