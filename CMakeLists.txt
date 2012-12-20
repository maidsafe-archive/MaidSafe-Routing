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
include_directories(${passport_SOURCE_DIR}/include)


###################################################################################################
# Set up all files as GLOBs                                                                       #
###################################################################################################
####    CODE    ####
file(GLOB SOURCE_FILES_ROUTING_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/*.cc"
                                   "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/routing_network.cc"
                                   "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/test_utils.cc")
file(GLOB HEADER_FILES_ROUTING_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/*.h"
                                   "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/test_utils.h")
file(GLOB PROTO_FILES_ROUTING_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/*.proto")
file(GLOB SOURCE_PROTO_FILES_ROUTING_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/*.pb.cc")
file(GLOB HEADER_PROTO_FILES_ROUTING_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/*pb.h")
file(GLOB API_FILES_ROUTING_DIR "${PROJECT_SOURCE_DIR}/include/maidsafe/routing/*.h"
                                "${PROJECT_SOURCE_DIR}/include/maidsafe/routing/tests/*.h")

source_group("Routing Source Files" FILES ${SOURCE_FILES_ROUTING_DIR})
source_group("Routing Header Files" FILES ${HEADER_FILES_ROUTING_DIR})
source_group("Routing Proto Files" FILES ${PROTO_FILES_ROUTING_DIR} ${SOURCE_PROTO_FILES_ROUTING_DIR} ${HEADER_PROTO_FILES_ROUTING_DIR})
source_group("Routing API Files" FILES ${API_FILES_ROUTING_DIR})

set(ALL_FILES_ROUTING_DIR ${SOURCE_FILES_ROUTING_DIR} ${HEADER_FILES_ROUTING_DIR} ${PROTO_FILES_ROUTING_DIR} ${API_FILES_ROUTING_DIR})

set(STYLE_ROUTING ${ALL_FILES_ROUTING_DIR})
list(REMOVE_ITEM STYLE_ROUTING ${SOURCE_PROTO_FILES_ROUTING_DIR} ${HEADER_PROTO_FILES_ROUTING_DIR} ${PROTO_FILES_ROUTING_DIR})
list(REMOVE_ITEM SOURCE_FILES_ROUTING_DIR ${SOURCE_PROTO_FILES_ROUTING_DIR})
list(REMOVE_ITEM HEADER_FILES_ROUTING_DIR ${HEADER_PROTO_FILES_ROUTING_DIR})

####    TESTS    ####
file(GLOB SOURCE_FILES_ROUTING_TESTS_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/*.cc")
list(REMOVE_ITEM SOURCE_FILES_ROUTING_TESTS_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/routing_network.cc")
list(REMOVE_ITEM SOURCE_FILES_ROUTING_TESTS_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/test_utils.cc")
file(GLOB HEADER_FILES_ROUTING_TESTS_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/*.h")
list(REMOVE_ITEM HEADER_FILES_ROUTING_TESTS_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/test_utils.h")
set(ALL_FILES_ROUTING_TESTS_DIR ${SOURCE_FILES_ROUTING_TESTS_DIR} ${HEADER_FILES_ROUTING_TESTS_DIR})
set(STYLE_ROUTING ${STYLE_ROUTING} ${ALL_FILES_ROUTING_TESTS_DIR})
source_group("Routing Tests Source Files" FILES ${SOURCE_FILES_ROUTING_TESTS_DIR})
source_group("Routing Tests Header Files" FILES ${HEADER_FILES_ROUTING_TESTS_DIR})
list(REMOVE_ITEM ALL_FILES_ROUTING_TESTS_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/routing_api_test.cc"
                                             "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/find_nodes_test.cc"
                                             "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/routing_churn_test.cc"
                                             "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/routing_stand_alone_test.cc"
                                             "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/routing_functional_test.cc"
                                             "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/test_func_main.cc")
file(GLOB ALL_FILES_ROUTING_API_TEST "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/routing_api_test.cc"
                                     "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/test_main.cc")
file(GLOB ALL_FILES_ROUTING_FUNC_TESTS_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/find_nodes_test.cc"
                                           "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/routing_functional_test.cc"
                                           "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/test_func_main.cc")
file(GLOB ALL_FILES_ROUTING_BIG_TESTS_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/routing_churn_test.cc"
                                          "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/routing_stand_alone_test.cc"
                                          "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/test_main.cc")


###################################################################################################
# Define MaidSafe libraries and executables                                                       #
###################################################################################################
ms_add_static_library(routing ${ALL_FILES_ROUTING_DIR})
target_link_libraries(maidsafe_routing maidsafe_rudp maidsafe_passport)

if(MAIDSAFE_TESTING)
  ms_add_executable(TESTrouting "Tests/Routing" ${ALL_FILES_ROUTING_TESTS_DIR})
  ms_add_executable(TESTrouting_api "Tests/Routing" ${ALL_FILES_ROUTING_API_TEST})
  # new executable TESTrouting_func is created to contain func tests excluded from TESTrouting, can be run seperately
  ms_add_executable(TESTrouting_func "Tests/Routing" ${ALL_FILES_ROUTING_FUNC_TESTS_DIR})
  # new executable TESTrouting_big is created to contain tests that each need their own network
  ms_add_executable(TESTrouting_big "Tests/Routing" ${ALL_FILES_ROUTING_BIG_TESTS_DIR})
  ms_add_executable(create_lifestuff_bootstrap "Tools" "${PROJECT_SOURCE_DIR}/src/maidsafe/tools/create_bootstrap.cc")
  #  ms_add_executable(routing_key_helper "Tools" "${PROJECT_SOURCE_DIR}/src/maidsafe/tools/key_helper.cc")
  #  ms_add_executable(routing_node "Tools" "${PROJECT_SOURCE_DIR}/src/maidsafe/tools/routing_node.cc"
  #                                      "${PROJECT_SOURCE_DIR}/src/maidsafe/tools/commands.h"
  #                                       "${PROJECT_SOURCE_DIR}/src/maidsafe/tools/commands.cc")
  target_link_libraries(TESTrouting maidsafe_routing)
  target_link_libraries(TESTrouting_api maidsafe_routing)
  target_link_libraries(TESTrouting_func maidsafe_routing)
  target_link_libraries(TESTrouting_big maidsafe_routing)
  target_link_libraries(create_lifestuff_bootstrap maidsafe_routing)
  #target_link_libraries(routing_key_helper maidsafe_routing)
  #  target_link_libraries(routing_node maidsafe_routing)
endif()

rename_outdated_built_exes()

###################################################################################################
# Set compiler and linker flags                                                                   #
###################################################################################################
include(maidsafe_standard_flags)

if(QA_BUILD)
  add_definitions(-DQA_BUILD)
endif()

ms_disable_warnings(${SOURCE_PROTO_FILES_ROUTING_DIR})
if(UNIX)
  set_source_files_properties(
      "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/mock_routing_table.cc"
      "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/mock_network_utils.cc"
      "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/mock_service.cc"
      "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/mock_response_handler.cc"
      PROPERTIES COMPILE_FLAGS "-Wno-effc++")
endif()

if(UNIX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-effc++")
endif()

###################################################################################################
# Tests                                                                                           #
###################################################################################################
if(MAIDSAFE_TESTING)
  add_style_test(${STYLE_ROUTING})
  add_gtests(TESTrouting)
  add_gtests(TESTrouting_api)
  label_as_critical_tests(APITest.BEH_API_ZeroState
                          APITest.FUNC_API_AnonymousNode
                          APITest.BEH_API_SendToSelf
                          APITest.BEH_API_ClientNode
                          APITest.BEH_API_ClientNodeSameId
                          APITest.BEH_API_NodeNetwork
                          APITest.BEH_API_NodeNetworkWithClient
                          )
  add_project_experimental()
  test_summary_output()
endif()
