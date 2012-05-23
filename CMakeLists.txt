#==============================================================================#
#                                                                              #
#  Copyright (c) 2012 MaidSafe.net limited                                     #
#                                                                              #
#  The following source code is property of MaidSafe.net limited and is not    #
#  meant for external use.  The use of this code is governed by the license    #
#  file licence.txt found in the root directory of this project and also on    #
#  www.maidsafe.net.                                                           #
#                                                                              #
#  You are not free to copy, amend or otherwise use this source code without   #
#  the explicit written permission of the board of directors of MaidSafe.net.  #
#                                                                              #
#==============================================================================#


set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

cmake_minimum_required(VERSION 2.8.7 FATAL_ERROR)
project(routing)

set(PROTO_SOURCE_DIR ${PROJECT_SOURCE_DIR}/src)
file(GLOB PROTO_FILES RELATIVE ${PROTO_SOURCE_DIR} "${PROTO_SOURCE_DIR}/maidsafe/routing/*.proto")
include(../../cmake_modules/maidsafe_standard_setup.cmake)

include_directories(${common_SOURCE_DIR}/include)
include_directories(${rudp_SOURCE_DIR}/include)


###################################################################################################
# Set up all files as GLOBs                                                                       #
###################################################################################################
####    CODE    ####
file(GLOB SOURCE_FILES_ROUTING_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/*.cc")
file(GLOB HEADER_FILES_ROUTING_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/*.h")
file(GLOB PROTO_FILES_ROUTING_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/*.proto")
file(GLOB SOURCE_PROTO_FILES_ROUTING_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/*.pb.cc")
file(GLOB HEADER_PROTO_FILES_ROUTING_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/*pb.h")
file(GLOB API_FILES_ROUTING_DIR "${PROJECT_SOURCE_DIR}/include/maidsafe/routing/*.h")
set(ALL_FILES_ROUTING_DIR ${SOURCE_FILES_ROUTING_DIR} ${HEADER_FILES_ROUTING_DIR} ${PROTO_FILES_ROUTING_DIR} ${API_FILES_ROUTING_DIR})
set(STYLE_ROUTING ${ALL_FILES_ROUTING_DIR})
list(REMOVE_ITEM STYLE_ROUTING ${SOURCE_PROTO_FILES_ROUTING_DIR} ${HEADER_PROTO_FILES_ROUTING_DIR} ${PROTO_FILES_ROUTING_DIR})
list(REMOVE_ITEM SOURCE_FILES_ROUTING_DIR ${SOURCE_PROTO_FILES_ROUTING_DIR})
list(REMOVE_ITEM HEADER_FILES_ROUTING_DIR ${HEADER_PROTO_FILES_ROUTING_DIR})
source_group("Routing Source Files" FILES ${SOURCE_FILES_ROUTING_DIR})
source_group("Routing Header Files" FILES ${HEADER_FILES_ROUTING_DIR})
source_group("Routing Proto Files" FILES ${PROTO_FILES_ROUTING_DIR} ${SOURCE_PROTO_FILES_ROUTING_DIR} ${HEADER_PROTO_FILES_ROUTING_DIR})
source_group("Routing API Files" FILES ${API_FILES_ROUTING_DIR})

####    TESTS    ####
file(GLOB SOURCE_FILES_ROUTING_TESTS_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/*.cc")
file(GLOB HEADER_FILES_ROUTING_TESTS_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/*.h")
set(ALL_FILES_ROUTING_TESTS_DIR ${SOURCE_FILES_ROUTING_TESTS_DIR} ${HEADER_FILES_ROUTING_TESTS_DIR})
set(STYLE_ROUTING_TESTS ${ALL_FILES_ROUTING_TESTS_DIR})
source_group("Routing Tests Source Files" FILES ${SOURCE_FILES_ROUTING_TESTS_DIR})
source_group("Routing Tests Header Files" FILES ${HEADER_FILES_ROUTING_TESTS_DIR})


###################################################################################################
# Define MaidSafe libraries and executables                                                       #
###################################################################################################
ms_add_static_library(routing ${ALL_FILES_ROUTING_DIR})
ms_add_executable(TESTrouting "Tests/Routing" ${ALL_FILES_ROUTING_TESTS_DIR})

target_link_libraries(maidsafe_routing maidsafe_rudp)
target_link_libraries(TESTrouting maidsafe_routing)

rename_outdated_built_exes()


###################################################################################################
# Set compiler and linker flags                                                                   #
###################################################################################################
include(maidsafe_standard_flags)
ms_disable_warnings(${SOURCE_PROTO_FILES_ROUTING_DIR})


###################################################################################################
# Tests                                                                                           #
###################################################################################################
add_style_test(${STYLE_ROUTING} ${STYLE_ROUTING_TESTS})
add_gtests(TESTrouting)
test_summary_output()
