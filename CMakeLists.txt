cmake_minimum_required(VERSION 3.21)
project(d3hack C CXX ASM)

if(NOT SWITCH)
    message(FATAL_ERROR "Not targeting switch, make sure to specify -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain.cmake")
endif()

if(DEFINED CMAKE_TOOLCHAIN_FILE)
    get_filename_component(EXL_TOOLCHAIN_FILE "${CMAKE_TOOLCHAIN_FILE}" ABSOLUTE)
    if(NOT EXISTS "${EXL_TOOLCHAIN_FILE}")
        message(FATAL_ERROR "CMAKE_TOOLCHAIN_FILE not found: ${EXL_TOOLCHAIN_FILE}")
    endif()
endif()

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

include(${CMAKE_SOURCE_DIR}/cmake/ConfigMk.cmake)

set(EXL_CONFIG_VARS
    LOAD_KIND
    PROGRAM_ID
    CONFIG_JSON
    C_FLAGS
    CXX_FLAGS
    ELF_EXTRACT
    PYTHON
    NPDM_JSON
    MOUNT_PATH
    FTP_IP
    FTP_PORT
    FTP_USERNAME
    FTP_PASSWORD
    RYU_WIN_PATH
    RYU_MAC_PATH
    YUZU_PATH
)

exl_load_config_mk(PATH "${CMAKE_SOURCE_DIR}/config.mk" VARS ${EXL_CONFIG_VARS})

if(EXISTS "${CMAKE_SOURCE_DIR}/config.cmake")
    include("${CMAKE_SOURCE_DIR}/config.cmake")
endif()

include(${CMAKE_SOURCE_DIR}/cmake/Iwyu.cmake)
exl_configure_iwyu()

if(NOT DEFINED EXL_TARGET_NAME OR EXL_TARGET_NAME STREQUAL "")
    get_filename_component(EXL_TARGET_NAME "${CMAKE_SOURCE_DIR}" NAME)
endif()

if(NOT DEFINED LOAD_KIND OR LOAD_KIND STREQUAL "")
    set(LOAD_KIND "Module")
endif()

if(NOT DEFINED PROGRAM_ID OR PROGRAM_ID STREQUAL "")
    message(FATAL_ERROR "PROGRAM_ID not set; edit config.mk or pass -DPROGRAM_ID=...")
endif()

if(DEFINED CONFIG_JSON AND NOT CONFIG_JSON STREQUAL "")
    set(EXL_CONFIG_JSON "${CMAKE_SOURCE_DIR}/${CONFIG_JSON}")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/${EXL_TARGET_NAME}.json")
    set(EXL_CONFIG_JSON "${CMAKE_SOURCE_DIR}/${EXL_TARGET_NAME}.json")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/config.json")
    set(EXL_CONFIG_JSON "${CMAKE_SOURCE_DIR}/config.json")
endif()

if(NOT EXL_CONFIG_JSON OR NOT EXISTS "${EXL_CONFIG_JSON}")
    message(FATAL_ERROR "CONFIG_JSON not found; edit config.mk (CONFIG_JSON) or add a config.json")
endif()

if(LOAD_KIND STREQUAL "Module")
    set(EXL_LOAD_KIND_ENUM 2)
    set(EXL_SPECS_TEMPLATE "${CMAKE_SOURCE_DIR}/cmake/module.specs.template")
    set(EXL_BINARY_NAME "subsdk9")
elseif(LOAD_KIND STREQUAL "AsRtld")
    set(EXL_LOAD_KIND_ENUM 1)
    set(EXL_SPECS_TEMPLATE "${CMAKE_SOURCE_DIR}/cmake/as_rtld.specs.template")
    set(EXL_BINARY_NAME "rtld")
else()
    message(FATAL_ERROR "LOAD_KIND is invalid, please check config.mk")
endif()

set(EXL_PROGRAM_ID "${PROGRAM_ID}")
string(REGEX REPLACE "^0[xX]" "" EXL_PROGRAM_ID "${EXL_PROGRAM_ID}")

get_filename_component(EXL_LINK_LD "${CMAKE_SOURCE_DIR}/misc/link.ld" ABSOLUTE)
execute_process(COMMAND uname OUTPUT_VARIABLE EXL_UNAME)
if(EXL_UNAME MATCHES "^MINGW")
    string(REGEX REPLACE "^/([a-zA-Z])/" "\\1:/" EXL_LINK_LD "${EXL_LINK_LD}")
endif()
configure_file(${EXL_SPECS_TEMPLATE} ${CMAKE_BINARY_DIR}/exl.specs @ONLY)

file(GLOB_RECURSE EXL_SOURCES_C CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/source/*.c)
file(GLOB_RECURSE EXL_SOURCES_CXX CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/source/*.cpp)
file(GLOB_RECURSE EXL_SOURCES_ASM CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/source/*.s)

list(FILTER EXL_SOURCES_CXX EXCLUDE REGEX "/source/third_party/imgui/examples/")
list(FILTER EXL_SOURCES_CXX EXCLUDE REGEX "/source/third_party/imgui/backends/")
list(FILTER EXL_SOURCES_CXX EXCLUDE REGEX "/source/third_party/imgui/docs/")
list(FILTER EXL_SOURCES_CXX EXCLUDE REGEX "/source/third_party/imgui/misc/")
list(FILTER EXL_SOURCES_CXX EXCLUDE REGEX "/source/third_party/imgui/.*imgui_impl_.*\\.cpp$")
list(FILTER EXL_SOURCES_CXX EXCLUDE REGEX "/imgui_demo\\.cpp$")
list(FILTER EXL_SOURCES_CXX EXCLUDE REGEX "/imgui_freetype\\.cpp$")
list(FILTER EXL_SOURCES_CXX EXCLUDE REGEX "/imgui_stdlib\\.cpp$")
list(FILTER EXL_SOURCES_CXX EXCLUDE REGEX "/binary_to_compressed_c\\.cpp$")
list(FILTER EXL_SOURCES_CXX EXCLUDE REGEX "/example_.*\\.cpp$")

list(FILTER EXL_SOURCES_C EXCLUDE REGEX "/source/third_party/imgui/examples/")
list(FILTER EXL_SOURCES_C EXCLUDE REGEX "/source/third_party/imgui/backends/")
list(FILTER EXL_SOURCES_C EXCLUDE REGEX "/source/third_party/imgui/docs/")
list(FILTER EXL_SOURCES_C EXCLUDE REGEX "/source/third_party/imgui/misc/")
list(FILTER EXL_SOURCES_C EXCLUDE REGEX "/uSynergy\\.c$")

list(APPEND EXL_SOURCES_CXX "${PROJECT_SOURCE_DIR}/source/third_party/imgui_backend/imgui_impl_nvn.cpp")
list(APPEND EXL_SOURCES_CXX "${PROJECT_SOURCE_DIR}/source/third_party/imgui_backend/imgui_impl_nvn_textures.cpp")
list(REMOVE_DUPLICATES EXL_SOURCES_CXX)

add_executable(${EXL_TARGET_NAME} ${EXL_SOURCES_ASM} ${EXL_SOURCES_C} ${EXL_SOURCES_CXX})
exl_apply_iwyu(${EXL_TARGET_NAME})
exl_add_find_all_symbols_target(iwyu-symbols)

set(EXL_INCLUDE_DIRS
    "${PROJECT_SOURCE_DIR}/include"
    # "${PROJECT_SOURCE_DIR}/source" # Switch source to only non-system include
)

file(GLOB EXL_MODULE_DIRS LIST_DIRECTORIES true "${PROJECT_SOURCE_DIR}/source/*")
foreach(dir IN LISTS EXL_MODULE_DIRS)
    if(IS_DIRECTORY "${dir}")
        list(APPEND EXL_INCLUDE_DIRS "${dir}")
    endif()
endforeach()

if(LIBNX)
    list(APPEND EXL_INCLUDE_DIRS "${LIBNX}/include")
endif()

target_include_directories(${EXL_TARGET_NAME} PRIVATE "${PROJECT_SOURCE_DIR}/source")
target_include_directories(${EXL_TARGET_NAME} SYSTEM PRIVATE ${EXL_INCLUDE_DIRS})
# if(LIBNX)
#     target_include_directories(${EXL_TARGET_NAME} SYSTEM PRIVATE "${LIBNX}/include")
# endif()

target_compile_definitions(${EXL_TARGET_NAME} PRIVATE
    IMGUI_USER_CONFIG=\"imgui_backend/nvn_imgui_config.h\"
    EXL_LOAD_KIND=${LOAD_KIND}
    EXL_LOAD_KIND_ENUM=${EXL_LOAD_KIND_ENUM}
    EXL_PROGRAM_ID=0x${EXL_PROGRAM_ID}
)

if(DEFINED C_FLAGS AND NOT C_FLAGS STREQUAL "")
    separate_arguments(EXL_USER_C_FLAGS NATIVE_COMMAND "${C_FLAGS}")
    target_compile_options(${EXL_TARGET_NAME} PRIVATE $<$<COMPILE_LANGUAGE:C>:${EXL_USER_C_FLAGS}>)
    target_compile_options(${EXL_TARGET_NAME} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${EXL_USER_C_FLAGS}>)
endif()

if(DEFINED CXX_FLAGS AND NOT CXX_FLAGS STREQUAL "")
    separate_arguments(EXL_USER_CXX_FLAGS NATIVE_COMMAND "${CXX_FLAGS}")
    target_compile_options(${EXL_TARGET_NAME} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${EXL_USER_CXX_FLAGS}>)
endif()

include(${CMAKE_SOURCE_DIR}/cmake/SwitchTools.cmake)

file(GLOB EXL_BINARIES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/data/*.bin")
if(EXL_BINARIES)
    target_embed_binaries(${EXL_TARGET_NAME} ${EXL_BINARIES})
endif()

add_nso_target(${EXL_TARGET_NAME})
set_target_properties(${EXL_TARGET_NAME} PROPERTIES LINK_FLAGS "")
set_target_properties(${EXL_TARGET_NAME} PROPERTIES CONFIG_JSON "${EXL_CONFIG_JSON}")
__generate_npdm(${EXL_TARGET_NAME})

if(DEFINED ARCH)
    separate_arguments(EXL_ARCH_FLAGS NATIVE_COMMAND "${ARCH}")
endif()

set(EXL_LINK_FLAGS
    "-specs=${CMAKE_BINARY_DIR}/exl.specs"
    "-g"
    ${EXL_ARCH_FLAGS}
    "-Wl,-Map,${CMAKE_BINARY_DIR}/${EXL_TARGET_NAME}.map"
    "-nostartfiles"
)

target_link_options(${EXL_TARGET_NAME} PRIVATE ${EXL_LINK_FLAGS})

set(EXL_OUT_DIR "${CMAKE_SOURCE_DIR}/deploy")
set(EXL_SD_OUT "atmosphere/contents/${PROGRAM_ID}")
set(EXL_NPDM_JSON_PATH "${CMAKE_SOURCE_DIR}/misc/npdm-json")

set(EXL_SCRIPT_ENV
    "NAME=${EXL_TARGET_NAME}"
    "OUT=${EXL_OUT_DIR}"
    "SD_OUT=${EXL_SD_OUT}"
    "PROGRAM_ID=${PROGRAM_ID}"
    "BINARY_NAME=${EXL_BINARY_NAME}"
    "ELF_EXTRACT=${ELF_EXTRACT}"
    "PYTHON=${PYTHON}"
    "NPDM_JSON=${NPDM_JSON}"
    "NPDM_JSON_PATH=${EXL_NPDM_JSON_PATH}"
    "MOUNT_PATH=${MOUNT_PATH}"
    "FTP_IP=${FTP_IP}"
    "FTP_PORT=${FTP_PORT}"
    "FTP_USERNAME=${FTP_USERNAME}"
    "FTP_PASSWORD=${FTP_PASSWORD}"
    "RYU_WIN_PATH=${RYU_WIN_PATH}"
    "RYU_MAC_PATH=${RYU_MAC_PATH}"
    "YUZU_PATH=${YUZU_PATH}"
)

add_custom_target(exl-post-build
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/${EXL_TARGET_NAME} ${CMAKE_SOURCE_DIR}/${EXL_TARGET_NAME}.elf
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/${EXL_TARGET_NAME}.nso ${CMAKE_SOURCE_DIR}/${EXL_TARGET_NAME}.nso
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/${EXL_TARGET_NAME}.npdm ${CMAKE_SOURCE_DIR}/${EXL_TARGET_NAME}.npdm
    COMMAND ${CMAKE_COMMAND} -E env ${EXL_SCRIPT_ENV} ${CMAKE_SOURCE_DIR}/misc/scripts/post-build.sh
    USES_TERMINAL
)
add_dependencies(exl-post-build ${EXL_TARGET_NAME}_nso create_npdm)

add_custom_target(npdm-json
    COMMAND ${CMAKE_COMMAND} -E env ${EXL_SCRIPT_ENV} ${CMAKE_SOURCE_DIR}/misc/scripts/make-npdm-json.py
    USES_TERMINAL
)

add_custom_target(deploy-sd
    COMMAND ${CMAKE_COMMAND} -E env ${EXL_SCRIPT_ENV} ${CMAKE_SOURCE_DIR}/misc/scripts/deploy-sd.sh
    USES_TERMINAL
)
add_custom_target(deploy-ftp
    COMMAND ${CMAKE_COMMAND} -E env ${EXL_SCRIPT_ENV} ${CMAKE_SOURCE_DIR}/misc/scripts/deploy-ftp.py
    USES_TERMINAL
)
add_custom_target(deploy-ryu
    COMMAND ${CMAKE_COMMAND} -E env ${EXL_SCRIPT_ENV} ${CMAKE_SOURCE_DIR}/misc/scripts/deploy-ryu.sh
    USES_TERMINAL
)
add_custom_target(deploy-yuzu
    COMMAND ${CMAKE_COMMAND} -E env ${EXL_SCRIPT_ENV} ${CMAKE_SOURCE_DIR}/misc/scripts/deploy-yuzu.sh
    USES_TERMINAL
)
add_custom_target(ryu-launch-log
    COMMAND ${CMAKE_COMMAND} -E env ${EXL_SCRIPT_ENV} ${CMAKE_SOURCE_DIR}/misc/scripts/ryu-launch-log.sh
    USES_TERMINAL
)
add_custom_target(ryu-screenshot
    COMMAND ${CMAKE_COMMAND} -E env ${EXL_SCRIPT_ENV} ${CMAKE_SOURCE_DIR}/misc/scripts/ryu-screenshot.sh
    USES_TERMINAL
)
add_custom_target(ryu-tail
    COMMAND ${CMAKE_COMMAND} -E env ${EXL_SCRIPT_ENV} ${CMAKE_SOURCE_DIR}/misc/scripts/ryu-launch-log.sh
    USES_TERMINAL
)

add_dependencies(deploy-sd exl-post-build)
add_dependencies(deploy-ftp exl-post-build)
add_dependencies(deploy-ryu exl-post-build)
add_dependencies(deploy-yuzu exl-post-build)
add_dependencies(ryu-tail deploy-ryu)
