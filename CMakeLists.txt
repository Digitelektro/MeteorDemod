cmake_minimum_required(VERSION 3.5)

include(ExternalProject)

project(Meteordemod LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")


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

find_package(OpenCV REQUIRED PATHS "D:/Programozas/opencv/own_build/install/")

link_directories(
    ${CMAKE_BINARY_DIR}/sgp4-build/libsgp4
    ${CMAKE_BINARY_DIR}/libcorrect-build/lib
)

add_executable(meteordemod 
    main.cpp
    imageproc/spreadimage.cpp
    imageproc/spreadimage.h
    imageproc/threatimage.cpp
    imageproc/threatimage.h
    decoder/bitio.cpp
    decoder/bitio.h
    decoder/correlation.cpp
    decoder/correlation.h
    decoder/meteorimage.cpp
    decoder/meteorimage.h
    decoder/packetparser.cpp
    decoder/packetparser.h
    decoder/reedsolomon.cpp
    decoder/reedsolomon.h
    decoder/viterbi.cpp
    decoder/viterbi.h
    tools/settings.cpp
    tools/settings.h
    tools/matrix.cpp
    tools/matrix.h
    tools/tlereader.cpp
    tools/tlereader.h
    tools/vector.cpp
    tools/vector.h
    tools/pixelgeolocationcalculator.cpp
    tools/pixelgeolocationcalculator.h
    GIS/shapereader.cpp
    GIS/shapereader.h
    GIS/shaperenderer.cpp
    GIS/shaperenderer.h
)

include_directories(
    ${OpenCV_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/imageproc
    ${CMAKE_SOURCE_DIR}/decoder
    ${CMAKE_SOURCE_DIR}/tools
    ${CMAKE_SOURCE_DIR}/external/sgp4/libsgp4
    ${CMAKE_SOURCE_DIR}/external/libcorrect/include
)

add_dependencies(meteordemod sgp4)
add_dependencies(meteordemod libcorrect)    

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
    )
endif()

if(WIN32)
    install(TARGETS meteordemod DESTINATION ${CMAKE_INSTALL_PREFIX})
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/resources/ DESTINATION ${CMAKE_INSTALL_PREFIX}/resources)
else()
    install(TARGETS meteordemod DESTINATION bin)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/resources/ DESTINATION $ENV{HOME}/meteordemod)
endif()
