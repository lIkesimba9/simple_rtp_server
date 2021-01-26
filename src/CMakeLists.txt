project(simple_rtp_server LANGUAGES CXX)
cmake_minimum_required(VERSION 3.16)


set(SOURCE main.cpp )

find_package(PkgConfig REQUIRED)

pkg_check_modules(GST REQUIRED gstreamer-1.0)
include_directories(${GST_INCLUDE_DIRS})
link_directories(${GST_LIBRARY_DIRS})
add_definitions(${GST_CFLAGS_OTHER})

pkg_check_modules(GST_RTP REQUIRED gstreamer-rtp-1.0)
include_directories(${GST_RTP_INCLUDE_DIRS})
link_directories(${GST_RTP_LIBRARY_DIRS})
add_definitions(${GST_RTP_CFLAGS_OTHER})

pkg_check_modules(GST_RTP_BASE REQUIRED gstreamer-plugins-base-1.0)
include_directories(${GST_RTP_BASE_INCLUDE_DIRS})
link_directories(${GST_RTP_BASE_LIBRARY_DIRS})
add_definitions(${GST_RTP_BASE_CFLAGS_OTHER})

pkg_check_modules(GST_RTP_GOOD REQUIRED gstreamer-plugins-good-1.0)
include_directories(${GST_RTP_GOOD_INCLUDE_DIRS})
link_directories(${GST_RTP_GOOD_LIBRARY_DIRS})
add_definitions(${GST_RTP_GOOD_CFLAGS_OTHER})

#pkg_check_modules(GLIB REQUIRED glib-2.0)
#include_directories(${GLIB_INCLUDE_DIRS})
#link_directories(${GLIB_LIBRARY_DIRS})
#add_definitions(${GLIB_CFLAGS_OTHER})


set(LIBS ${LIBS} ${GST_RTP_LIBRARIES} ${GST_LIBRARIES} ${GST_RTP_BASE_LIBRARIES} ${GST_RTP_GOOD_LIBRARIES}) #${GLIB_LIBRARIES})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)



add_executable(simple_rtp_server ${SOURCE})
target_link_libraries(simple_rtp_server ${LIBS})

#install(TARGETS simple_rtp_client DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/..)
