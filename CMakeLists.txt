cmake_minimum_required(VERSION 3.12)
project(libmoonsound C)

set(CMAKE_C_STANDARD 99)

set(LIBVGM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/modules/libvgm)

option(MOONSOUND_BUILD_CLI "Build mwm2wav CLI tool" ON)
if(EMSCRIPTEN)
    set(MOONSOUND_BUILD_CLI OFF CACHE BOOL "Build mwm2wav CLI tool" FORCE)
endif()

include_directories(
    src
    ${LIBVGM_DIR}
    ${LIBVGM_DIR}/emu
    ${LIBVGM_DIR}/emu/cores
)

set(LIBVGM_SOURCES
    ${LIBVGM_DIR}/emu/SoundEmu.c
    ${LIBVGM_DIR}/emu/Resampler.c
    ${LIBVGM_DIR}/emu/panning.c
    ${LIBVGM_DIR}/emu/dac_control.c
    ${LIBVGM_DIR}/emu/logging.c
    ${LIBVGM_DIR}/emu/cores/ymf278b.c
    ${LIBVGM_DIR}/emu/cores/262intf.c
    ${LIBVGM_DIR}/emu/cores/nukedopl3.c
)

add_definitions(-DVGM_LITTLE_ENDIAN)
add_definitions(-DSNDDEV_SELECT -DSNDDEV_YMF278B -DSNDDEV_YMF262)
add_definitions(-DEC_YMF262_NUKED)

add_library(moonsound STATIC
    src/mwm2wav.c
    src/mwm_parser.c
    ${LIBVGM_SOURCES}
)

target_compile_definitions(moonsound PRIVATE LIBMOONSOUND_LIBRARY)
target_link_libraries(moonsound m)

if(MOONSOUND_BUILD_CLI)
    add_executable(mwm2wav
        src/mwm2wav.c
        src/mwm_parser.c
        ${LIBVGM_SOURCES}
    )

    target_link_libraries(mwm2wav m)

    # Copy the built binary to the project root
    add_custom_command(TARGET mwm2wav POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:mwm2wav>
                ${CMAKE_CURRENT_SOURCE_DIR}/mwm2wav
    )
endif()
