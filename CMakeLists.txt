cmake_minimum_required(VERSION 3.5)

include(ExternalProject)

project(meteordemod
    VERSION 2.6.1
    LANGUAGES CXX
)

configure_file(cmake/version.h.in version.h)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DVERSION_BUILD=${VERSION_BUILD_NUMBER}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVERSION_BUILD=${VERSION_BUILD_NUMBER}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

if (NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
endif()


if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.9.2")
    message(FATAL_ERROR "Insufficient gcc version, minimum required: 4.9.2, provided: " + "${CMAKE_CXX_COMPILER_VERSION}")
  endif()
endif()


ExternalProject_Add(sgp4
  #GIT_REPOSITORY    https://github.com/.....
  #GIT_TAG           master
  SOURCE_DIR        "${CMAKE_CURRENT_SOURCE_DIR}/external/sgp4"
  BINARY_DIR        "${CMAKE_BINARY_DIR}/sgp4-build"
  INSTALL_COMMAND   cmake -E echo "Skipping install step."
  CMAKE_ARGS        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
)

ExternalProject_Add(libcorrect
  #GIT_REPOSITORY    https://github.com/.....
  #GIT_TAG           master
  SOURCE_DIR        "${CMAKE_CURRENT_SOURCE_DIR}/external/libcorrect"
  BINARY_DIR        "${CMAKE_BINARY_DIR}/libcorrect-build"
  INSTALL_COMMAND   cmake -E echo "Skipping install step."
  CMAKE_ARGS        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
)

find_package(OpenCV)
find_package(OpenCL)

link_directories(
    ${CMAKE_BINARY_DIR}/sgp4-build/libsgp4
    ${CMAKE_BINARY_DIR}/libcorrect-build/lib
)

add_definitions(-D_USE_MATH_DEFINES -D_SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING)

add_executable(meteordemod 
    main.cpp
    imageproc/spreadimage.cpp
    imageproc/threatimage.cpp
    imageproc/projectimage.cpp
    imageproc/tps.cpp
    decoder/correlation.cpp
    decoder/reedsolomon.cpp
    decoder/viterbi.cpp
    decoder/deinterleaver.cpp
    decoder/meteordecoder.cpp
    decoder/protocol/ccsds.cpp
    decoder/protocol/vcdu.cpp
    decoder/protocol/lrpt/decoder.cpp
    decoder/protocol/lrpt/msumr/segment.cpp
    decoder/protocol/lrpt/msumr/bitio.cpp
    decoder/protocol/lrpt/msumr/image.cpp
    common/settings.cpp
    tools/matrix.cpp
    tools/tlereader.cpp
    tools/vector.cpp
    tools/pixelgeolocationcalculator.cpp
    tools/databuffer.cpp
    tools/iniparser.cpp
    tools/threadpool.cpp
    tools/opencl.cpp
    GIS/shapereader.cpp
    GIS/shaperenderer.cpp
    GIS/dbfilereader.cpp
    DSP/meteordemodulator.cpp
    DSP/agc.cpp
    DSP/pll.cpp
	DSP/meteorcostas.cpp
	DSP/phasecontrolloop.cpp
	DSP/polyphasebank.cpp
	DSP/mm.cpp
	DSP/window.cpp
	DSP/windowedsinc.cpp
    DSP/filter.cpp
    DSP/iqsource.cpp
    DSP/wavreader.cpp
)

include_directories(
    ${OpenCV_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/imageproc
    ${CMAKE_SOURCE_DIR}/decoder
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/tools
    ${CMAKE_SOURCE_DIR}/DSP
    ${CMAKE_SOURCE_DIR}/external/sgp4/libsgp4
    ${CMAKE_SOURCE_DIR}/external/libcorrect/include
)

target_include_directories(meteordemod PUBLIC
    "${PROJECT_BINARY_DIR}"
)

add_dependencies(meteordemod sgp4)
add_dependencies(meteordemod libcorrect)    

if(OpenCL_FOUND)
    target_compile_definitions(meteordemod PUBLIC OPENCL_FOUND=1)
    include_directories(${OpenCL_INCLUDE_DIRS})
    target_link_libraries(meteordemod ${OpenCL_LIBRARIES})
endif()

if(WIN32)
    target_link_libraries(meteordemod
        ${OpenCV_LIBS}
        sgp4.lib
        correct.lib
    )
else()
    target_link_libraries(meteordemod
        ${OpenCV_LIBS}
        sgp4.a
        correct.a
        stdc++fs
    )
endif()

if(WIN32)
    install(TARGETS meteordemod DESTINATION ${CMAKE_INSTALL_PREFIX})
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/resources/ DESTINATION ${CMAKE_INSTALL_PREFIX}/resources)
else()
    install(TARGETS meteordemod DESTINATION bin COMPONENT binaries)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/resources/ DESTINATION $ENV{HOME}/.config/meteordemod COMPONENT config
    USE_SOURCE_PERMISSIONS
    )
endif()

if(UNIX AND NOT APPLE)
    find_file (DEBIAN_FOUND debian_version debconf.conf PATHS /etc)
    if (DEBIAN_FOUND)
        SET(CPACK_GENERATOR "DEB")
        SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "Digitelektro")
        SET(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/Digitelektro/MeteorDemod")
        SET(CPACK_DEBIAN_PACKAGE_DESCRIPTION "Russian Meteor M2 weather satellite data decoder")
		SET(CPACK_DEBIAN_PACKAGE_DEPENDS "python3-opencv")
        INCLUDE(CPack)
    endif()
endif()
