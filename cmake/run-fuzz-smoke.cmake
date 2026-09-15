# Run each libFuzzer target for a bounded time, as a CI smoke test.
#
# Invoked by the FuzzSmoke test in CMakeLists.txt:
#   cmake -DFUZZ_SECONDS=20 -DFUZZ_BINARIES=a,b,c -DCORPUS_ROOT=... -P run-fuzz-smoke.cmake
#
# A bounded run rather than a fixed number of iterations, because the corpus is what
# carries regressions: -runs=1 replays it (the per-target tests do that), while this
# gives each target time to explore past it.

if(NOT DEFINED FUZZ_SECONDS)
    set(FUZZ_SECONDS 20)
endif()
if(NOT DEFINED FUZZ_BINARIES)
    message(FATAL_ERROR "FUZZ_BINARIES must list the fuzz targets to run")
endif()

string(REPLACE "," ";" _binaries "${FUZZ_BINARIES}")
set(_failed "")

foreach(_binary IN LISTS _binaries)
    get_filename_component(_name "${_binary}" NAME)
    set(_corpus "${CORPUS_ROOT}/${_name}")

    message(STATUS "fuzzing ${_name} for ${FUZZ_SECONDS}s (corpus: ${_corpus})")

    # -max_total_time bounds the run; -print_final_stats makes the saturated
    # coverage visible in the log so a target that is not exploring is noticeable.
    execute_process(
        COMMAND "${_binary}"
                "-max_total_time=${FUZZ_SECONDS}"
                "-print_final_stats=1"
                "-rss_limit_mb=2048"
                "${_corpus}"
        RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE _err
    )

    if(NOT _rc EQUAL 0)
        message(STATUS "${_name} FAILED (exit ${_rc})")
        # A crash lands on stderr with the reproducer; surface it rather than
        # swallowing it behind cmake's own error formatting.
        message(STATUS "---- ${_name} stderr ----\n${_err}")
        list(APPEND _failed "${_name}")
    else()
        # Report the last few lines: libFuzzer prints DONE plus the stats there.
        string(REGEX MATCH "(DONE|cov: [^\n]*|stat::[^\n]*)" _summary "${_err}")
        message(STATUS "${_name} ok${_summary}")
    endif()
endforeach()

if(_failed)
    message(FATAL_ERROR "fuzz targets failed: ${_failed}")
endif()

message(STATUS "fuzz smoke test passed")
