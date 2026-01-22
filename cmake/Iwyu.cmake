include_guard(GLOBAL)

function(exl_cache_default name type doc default)
    if(DEFINED ${name})
        set(${name} "${${name}}" CACHE ${type} "${doc}" FORCE)
    else()
        set(${name} "${default}" CACHE ${type} "${doc}")
    endif()
endfunction()

function(exl_shell_quote out value)
    set(_value "${value}")
    string(REPLACE "'" "'\"'\"'" _value "${_value}")
    set(${out} "'${_value}'" PARENT_SCOPE)
endfunction()

exl_cache_default(
    EXL_ENABLE_IWYU
    BOOL
    "Enable include-what-you-use (IWYU) on d3hack targets."
    OFF
)
exl_cache_default(
    EXL_IWYU_BINARY
    FILEPATH
    "Path to include-what-you-use binary. If empty, searches PATH."
    ""
)
exl_cache_default(
    EXL_IWYU_MAPPING_FILE
    FILEPATH
    "Path to IWYU mapping file (optional)."
    ""
)
exl_cache_default(
    EXL_IWYU_ARGS
    STRING
    "Extra IWYU args (CMake list; e.g. -Xiwyu;--verbose=3)."
    ""
)
exl_cache_default(
    EXL_IWYU_PYTHON
    FILEPATH
    "Python interpreter for IWYU helper scripts."
    "python3"
)
exl_cache_default(
    EXL_FIND_ALL_SYMBOLS_BINARY
    FILEPATH
    "Path to find-all-symbols binary (clang-tools-extra)."
    ""
)
exl_cache_default(
    EXL_IWYU_SYMBOLS_OUTPUT
    FILEPATH
    "Output path for merged find-all-symbols YAML."
    "${CMAKE_BINARY_DIR}/clang-tooling/find_all_symbols_db.yaml"
)
exl_cache_default(
    EXL_IWYU_SYMBOLS_ONLY_UNDER
    PATH
    "Only process files under this directory (optional)."
    "${PROJECT_SOURCE_DIR}/source"
)
exl_cache_default(
    EXL_IWYU_SYMBOLS_EXCLUDE
    STRING
    "Glob patterns to exclude (CMake list)."
    "${PROJECT_SOURCE_DIR}/source/third_party/*"
)
set(_exl_iwyu_symbols_extra_default "--extra-arg=-Wno-error;--extra-arg=-Wno-unknown-argument")
if(NOT DEFINED EXL_IWYU_SYMBOLS_EXTRA_ARGS OR EXL_IWYU_SYMBOLS_EXTRA_ARGS STREQUAL "")
    set(EXL_IWYU_SYMBOLS_EXTRA_ARGS
        "${_exl_iwyu_symbols_extra_default}"
        CACHE STRING
        "Extra args passed to run-find-all-symbols.py (CMake list)."
        FORCE
    )
else()
    set(EXL_IWYU_SYMBOLS_EXTRA_ARGS
        "${EXL_IWYU_SYMBOLS_EXTRA_ARGS}"
        CACHE STRING
        "Extra args passed to run-find-all-symbols.py (CMake list)."
        FORCE
    )
endif()
unset(_exl_iwyu_symbols_extra_default)

exl_cache_default(
    EXL_IWYU_TARGET
    STRING
    "Target triple passed to clang tooling (empty to skip)."
    "aarch64-none-elf"
)
exl_cache_default(
    EXL_IWYU_SYMBOLS_JOBS
    STRING
    "Parallel jobs for find-all-symbols (-j). Use 0 for auto."
    "0"
)

function(exl_configure_iwyu)
    if(NOT EXL_ENABLE_IWYU)
        return()
    endif()

    set(_needs_iwyu_cmd FALSE)
    if(NOT CMAKE_C_INCLUDE_WHAT_YOU_USE)
        set(_needs_iwyu_cmd TRUE)
    endif()
    if(NOT CMAKE_CXX_INCLUDE_WHAT_YOU_USE)
        set(_needs_iwyu_cmd TRUE)
    endif()
    if(NOT _needs_iwyu_cmd)
        return()
    endif()

    if(EXL_IWYU_BINARY)
        set(_exl_iwyu_bin "${EXL_IWYU_BINARY}")
    else()
        find_program(_exl_iwyu_bin NAMES include-what-you-use iwyu)
    endif()
    if(NOT _exl_iwyu_bin)
        message(FATAL_ERROR "IWYU enabled but include-what-you-use binary not found. Set EXL_IWYU_BINARY.")
    endif()

    set(_exl_iwyu_cmd "${_exl_iwyu_bin}")
    if(EXL_IWYU_MAPPING_FILE)
        list(APPEND _exl_iwyu_cmd "-Xiwyu" "--mapping_file=${EXL_IWYU_MAPPING_FILE}")
    endif()
    if(EXL_IWYU_ARGS)
        list(APPEND _exl_iwyu_cmd ${EXL_IWYU_ARGS})
    endif()

    if(NOT CMAKE_C_INCLUDE_WHAT_YOU_USE)
        set(CMAKE_C_INCLUDE_WHAT_YOU_USE "${_exl_iwyu_cmd}" CACHE STRING "IWYU command for C." FORCE)
    endif()
    if(NOT CMAKE_CXX_INCLUDE_WHAT_YOU_USE)
        set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE "${_exl_iwyu_cmd}" CACHE STRING "IWYU command for CXX." FORCE)
    endif()
endfunction()

function(exl_apply_iwyu target)
    if(CMAKE_C_INCLUDE_WHAT_YOU_USE)
        get_target_property(_exl_iwyu_c "${target}" C_INCLUDE_WHAT_YOU_USE)
        if(_exl_iwyu_c STREQUAL "NOTFOUND")
            set_property(
                TARGET "${target}"
                PROPERTY C_INCLUDE_WHAT_YOU_USE "${CMAKE_C_INCLUDE_WHAT_YOU_USE}"
            )
        endif()
    endif()

    if(CMAKE_CXX_INCLUDE_WHAT_YOU_USE)
        get_target_property(_exl_iwyu_cxx "${target}" CXX_INCLUDE_WHAT_YOU_USE)
        if(_exl_iwyu_cxx STREQUAL "NOTFOUND")
            set_property(
                TARGET "${target}"
                PROPERTY CXX_INCLUDE_WHAT_YOU_USE "${CMAKE_CXX_INCLUDE_WHAT_YOU_USE}"
            )
        endif()
    endif()
endfunction()

function(exl_add_find_all_symbols_target target_name)
    if(NOT target_name)
        message(FATAL_ERROR "exl_add_find_all_symbols_target requires a target name.")
    endif()

    set(_script "${PROJECT_SOURCE_DIR}/tools/clang-include-fixer/run-find-all-symbols.py")
    if(NOT EXISTS "${_script}")
        message(FATAL_ERROR "find-all-symbols script not found: ${_script}")
    endif()

    if(NOT EXL_IWYU_SYMBOLS_OUTPUT)
        message(FATAL_ERROR "EXL_IWYU_SYMBOLS_OUTPUT is empty.")
    endif()

    get_filename_component(_output_dir "${EXL_IWYU_SYMBOLS_OUTPUT}" DIRECTORY)

    set(_cmd "${EXL_IWYU_PYTHON}" "${_script}" "-p" "${CMAKE_BINARY_DIR}")
    list(APPEND _cmd "-saving-path" "${EXL_IWYU_SYMBOLS_OUTPUT}")

    if(EXL_FIND_ALL_SYMBOLS_BINARY)
        list(APPEND _cmd "-binary" "${EXL_FIND_ALL_SYMBOLS_BINARY}")
    endif()
    if(EXL_IWYU_SYMBOLS_JOBS)
        list(APPEND _cmd "-j" "${EXL_IWYU_SYMBOLS_JOBS}")
    endif()
    if(EXL_IWYU_TARGET)
        list(APPEND _cmd "--extra-arg-before=--target=${EXL_IWYU_TARGET}")
    endif()
    if(EXL_IWYU_SYMBOLS_ONLY_UNDER)
        exl_shell_quote(_only_under "${EXL_IWYU_SYMBOLS_ONLY_UNDER}")
        list(APPEND _cmd "--only-under" "${_only_under}")
    endif()
    if(EXL_IWYU_SYMBOLS_EXCLUDE)
        foreach(_pattern IN LISTS EXL_IWYU_SYMBOLS_EXCLUDE)
            exl_shell_quote(_exclude_pattern "${_pattern}")
            list(APPEND _cmd "--exclude" "${_exclude_pattern}")
        endforeach()
    endif()
    if(EXL_IWYU_SYMBOLS_EXTRA_ARGS)
        list(APPEND _cmd ${EXL_IWYU_SYMBOLS_EXTRA_ARGS})
    endif()

    add_custom_target(
        ${target_name}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_output_dir}"
        COMMAND ${_cmd}
        BYPRODUCTS "${EXL_IWYU_SYMBOLS_OUTPUT}"
        USES_TERMINAL
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )
endfunction()
