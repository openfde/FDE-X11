cmake_minimum_required(VERSION 3.22)
project(lorie)

add_link_options("-Wl,-z,max-page-size=65536")

if(APP_LOG)
    add_compile_definitions(APP_LOG=1)
    add_compile_definitions(LOGGING_ENABLED=1)
else()
    add_compile_definitions(APP_LOG=0)
    add_compile_definitions(LOGGING_ENABLED=0)
endif()
set(can_use_assembler TRUE)
enable_language(ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE RelWithDebInfo)

include(TestBigEndian)
include(CheckIncludeFile)
include(CheckSourceCompiles)
include(CheckCCompilerFlag)
include(CheckSymbolExists)
include(CheckFunctionExists)
include(CheckTypeSize)

find_package(Python3 REQUIRED)
find_package(BISON REQUIRED)

# Applies the patches in patches/ to the vendored third party sources.
#
# The original implementation shelled out to
#   bash -c "patch -p1 -d <dir> -i <patch> -R --dry-run || patch -p1 -N -d <dir> -i <patch> -r -"
# which requires a POSIX shell and GNU patch, so it silently did nothing on a
# Windows host. The patches are already applied to the sources in this
# repository, so on a clean checkout this is a no-op; it only does real work if
# a vendored source tree is ever reverted or updated.
function(target_apply_patch patch_target d p)
    file(RELATIVE_PATH patch_dir "${CMAKE_SOURCE_DIR}" "${d}")
    get_filename_component(patch_name "${p}" NAME)

    find_program(GIT_EXECUTABLE git)
    if(GIT_EXECUTABLE)
        # Same check as "patch -R --dry-run": if the reverse patch applies, the
        # patch is already in place and there is nothing to do.
        execute_process(
                COMMAND "${GIT_EXECUTABLE}" apply -p1 "--directory=${patch_dir}" -R --check "${p}"
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                RESULT_VARIABLE patch_applied
                OUTPUT_QUIET ERROR_QUIET)
        if(patch_applied EQUAL 0)
            return()
        endif()

        execute_process(
                COMMAND "${GIT_EXECUTABLE}" apply -p1 "--directory=${patch_dir}" "${p}"
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                RESULT_VARIABLE patch_result
                OUTPUT_QUIET ERROR_QUIET)
        if(NOT patch_result EQUAL 0)
            message(WARNING "Could not apply ${patch_name} to ${patch_dir}")
        endif()
        return()
    endif()

    find_program(PATCH_EXECUTABLE patch)
    if(PATCH_EXECUTABLE)
        execute_process(
                COMMAND "${PATCH_EXECUTABLE}" -p1 -d "${d}" -i "${p}" -R --dry-run
                RESULT_VARIABLE patch_applied
                OUTPUT_QUIET ERROR_QUIET)
        if(NOT patch_applied EQUAL 0)
            execute_process(
                    COMMAND "${PATCH_EXECUTABLE}" -p1 -N -d "${d}" -i "${p}" -r -
                    RESULT_VARIABLE patch_result
                    OUTPUT_QUIET ERROR_QUIET)
            if(NOT patch_result EQUAL 0)
                message(WARNING "Could not apply ${patch_name} to ${patch_dir}")
            endif()
        endif()
        return()
    endif()

    message(STATUS "Neither git nor patch was found, assuming ${patch_name} is already applied to ${patch_dir}")
endfunction()

set(test_compile_options
        "-Wall"
        "-Wpointer-arith"
        "-Wmissing-declarations"
        "-Wformat=2"
        "-Winvalid-pch"
        "-Wstrict-prototypes"
        "-Wmissing-prototypes"
        "-Wnested-externs"
        "-Wbad-function-cast"
        "-Wold-style-definition"
        "-Wunused"
        "-Wuninitialized"
        "-Wshadow"
        "-Wmissing-noreturn"
        "-Wmissing-format-attribute"
        "-Wredundant-decls"
        "-Wlogical-op"
        "-Werror=implicit"
        "-Werror=nonnull"
        "-Werror=init-self"
        "-Werror=main"
        "-Werror=missing-braces"
        "-Werror=sequence-point"
        "-Werror=return-type"
        "-Werror=trigraphs"
        "-Werror=array-bounds"
        "-Werror=write-strings"
        "-Werror=address"
        "-Werror=int-to-pointer-cast"
        "-Werror=pointer-to-int-cast"
        "-fno-strict-aliasing"
        "-Wno-unused-but-set-variable"
        "-Wno-unused-parameter"
        "-Wno-unused-variable")

set(CMAKE_REQUIRED_QUIET_OLD ${CMAKE_REQUIRED_QUIET})
set(CMAKE_REQUIRED_QUIET 1)
set(common_compile_options)
set(I 0)
foreach (option ${test_compile_options})
    check_c_compiler_flag(${option} COMPILER_TEST_${I})
    if (COMPILER_TEST_${I})
        set(common_compile_options ${common_compile_options} ${option})
    endif()
    MATH(EXPR I "${I}+1")
endforeach ()
set(CMAKE_REQUIRED_QUIET ${CMAKE_REQUIRED_QUIET_OLD})

foreach(project xorgproto fontenc md pixman tirpc xshmfence Xdmcp Xau Xfont2 xkbcomp xserver)
    include(recipes/${project}.cmake)
endforeach()
