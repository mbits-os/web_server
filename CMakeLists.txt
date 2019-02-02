CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(http_server C CXX)

set(WEB_SERVER_WALL_FLAGS ON CACHE BOOL "Compile with -Wall/-W4 warning levels")

if (WEB_SERVER_WALL_FLAGS)
  if (MSVC)
    set(ADDITIONAL_WALL_FLAGS
        /std:c++latest
        /permissive-
        /W4
        /w14242
        /w14254
        /w14263
        /w14265
        /w14287
        /we4289
        /w14296
        /w14311
        /w14545
        /w14546
        /w14547
        /w14549
        /w14555
        /w14619
        /w14640
        /w14826
        /w14905
        /w14906
        /w14928
        /w14946)
  else()
    set(ADDITIONAL_WALL_FLAGS
        -std=c++17
        -Wall -Wextra
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
    )
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
      list(APPEND ADDITIONAL_WALL_FLAGS -fcolor-diagnostics) # -Wlifetime
    else()
      list(APPEND ADDITIONAL_WALL_FLAGS
        -fdiagnostics-color
        -Wmisleading-indentation
        -Wduplicated-cond
        -Wduplicated-branches
        -Wlogical-op
        -Wuseless-cast
        )
    endif()
  endif()
endif()

SET(PREFIX ${CMAKE_INSTALL_PREFIX})
SET(EXEC_PREFIX "${PREFIX}")
SET(INCLUDEDIR "${PREFIX}/include")
SET(USRLIBDIR "${PREFIX}/lib")
SET(USRBINDIR "${PREFIX}/bin")
SET(VERSION 0.0.1)

SET(SRCS
    src/asio.cc
    src/headers.cc
    src/log.cc
    src/mime_type.cc
    src/path_compiler.cc
    src/request.cc
    src/request_parser.cc
    src/response.cc
    src/router.cc
    src/stream.cc
    src/server_common.cc
    src/uri.cc

    include/web/bits/asio.h
    include/web/delegate.h
    include/web/headers.h
    include/web/log.h
    include/web/middleware.h
    include/web/mime_type.h
    include/web/path_compiler.h
    include/web/request.h
    include/web/request_parser.h
    include/web/response.h
    include/web/route.h
    include/web/router.h
    include/web/server.h
    include/web/stream.h
    include/web/uri.h
)

INCLUDE(FindPkgConfig)

if (NOT Boost_FOUND)
set(Boost_USE_MULTITHREADED     ON)
IF (WIN32)
set(Boost_USE_STATIC_LIBS       ON)
set(Boost_USE_STATIC_RUNTIME    ON)
else()
SET(Boost_USE_STATIC_LIBS      OFF)
SET(Boost_USE_STATIC_RUNTIME   OFF)
endif()
find_package(Boost REQUIRED system)
LINK_DIRECTORIES(${Boost_LIBRARY_DIRS})
endif()

if (NOT Threads_FOUND)
find_package(Threads)
endif()

ADD_LIBRARY(http_server STATIC ${SRCS})
TARGET_INCLUDE_DIRECTORIES(http_server
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Boost_INCLUDE_DIRS})
TARGET_COMPILE_OPTIONS(http_server PUBLIC -DHTTP_USE_ASIO)
IF(MSVC)
TARGET_COMPILE_OPTIONS(http_server PUBLIC -D_WIN32_WINNT=0x0501 -D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING)
ENDIF()
TARGET_LINK_LIBRARIES(http_server ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
SET_TARGET_PROPERTIES(http_server PROPERTIES CXX_STANDARD 17 CXX_EXTENSIONS OFF)


ADD_LIBRARY(middleware_files STATIC middleware/files/files.h middleware/files/files.cc)
TARGET_INCLUDE_DIRECTORIES(middleware_files
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/middleware
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
SET_TARGET_PROPERTIES(middleware_files PROPERTIES CXX_STANDARD 17 CXX_EXTENSIONS OFF)
