CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(http_server C CXX)

SET(PREFIX ${CMAKE_INSTALL_PREFIX})
SET(EXEC_PREFIX "${PREFIX}")
SET(INCLUDEDIR "${PREFIX}/include")
SET(USRLIBDIR "${PREFIX}/lib")
SET(USRBINDIR "${PREFIX}/bin")
SET(VERSION 0.0.1)

SET(SRCS
    src/asio.cc
    src/headers.cc
    src/mime_type.cc
    src/path_compiler.cc
    src/request.cc
    src/request_parser.cc
    src/response.cc
    src/router.cc
    src/server_common.cc
    src/uri.cc

    include/web/bits/asio.h
    include/web/cstring.h
    include/web/delegate.h
    include/web/headers.h
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
endif()

if (NOT Threads_FOUND)
find_package(Threads)
endif()

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})
INCLUDE_DIRECTORIES("${CMAKE_CURRENT_SOURCE_DIR}/include")
INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})
LINK_DIRECTORIES(${Boost_LIBRARY_DIRS})

ADD_DEFINITIONS(-DHTTP_USE_ASIO)


IF (WIN32)
ADD_DEFINITIONS(-D_WIN32_WINNT=0x0501)
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${EXTRA_CFLAGS} /MTd")
SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${EXTRA_CFLAGS} /MT")
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO")
ELSE()
ADD_DEFINITIONS(-fno-rtti)
ENDIF()

ADD_LIBRARY(http_server STATIC ${SRCS})
TARGET_LINK_LIBRARIES(http_server ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
SET_PROPERTY(TARGET http_server PROPERTY CXX_STANDARD 14)
