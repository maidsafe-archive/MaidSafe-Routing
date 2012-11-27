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
include_directories(${private_SOURCE_DIR}/include)


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
list(REMOVE_ITEM ALL_FILES_ROUTING_TESTS_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/routing_api_test.cc")
file(GLOB ALL_FILES_ROUTING_API_TEST "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/routing_api_test.cc"
                                     "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/test_main.cc")


###################################################################################################
# Define MaidSafe libraries and executables                                                       #
###################################################################################################
ms_add_static_library(routing ${ALL_FILES_ROUTING_DIR})
target_link_libraries(maidsafe_routing maidsafe_rudp maidsafe_utilities)

if(MAIDSAFE_TESTING)
  ms_add_executable(TESTrouting "Tests/Routing" ${ALL_FILES_ROUTING_TESTS_DIR})
  ms_add_executable(TESTrouting_api "Tests/Routing" ${ALL_FILES_ROUTING_API_TEST})
  ms_add_executable(create_lifestuff_bootstrap "Tools" "${PROJECT_SOURCE_DIR}/src/maidsafe/tools/create_bootstrap.cc")
  ms_add_executable(routing_key_helper "Tools" "${PROJECT_SOURCE_DIR}/src/maidsafe/tools/key_helper.cc")
  ms_add_executable(routing_node "Tools" "${PROJECT_SOURCE_DIR}/src/maidsafe/tools/routing_node.cc"
                                         "${PROJECT_SOURCE_DIR}/src/maidsafe/tools/commands.h"
                                         "${PROJECT_SOURCE_DIR}/src/maidsafe/tools/commands.cc")
  target_link_libraries(TESTrouting maidsafe_routing)
  target_link_libraries(TESTrouting_api maidsafe_routing)
  target_link_libraries(create_lifestuff_bootstrap maidsafe_routing)
  target_link_libraries(routing_key_helper maidsafe_routing)
  target_link_libraries(routing_node maidsafe_routing)
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
      "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/mock_network_utils.cc"
      "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/mock_service.cc"
      "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/mock_response_handler.cc"
      PROPERTIES COMPILE_FLAGS "-Wno-effc++")
endif()


###################################################################################################
# Tests                                                                                           #
###################################################################################################
if(MAIDSAFE_TESTING)
  add_style_test(${STYLE_ROUTING})
  add_gtests(TESTrouting)
  add_gtests(TESTrouting_api)
  label_as_critical_tests(RoutingNetworkTest.FUNC_SetupNetwork
                          RoutingNetworkTest.FUNC_SetupSingleClientHybridNetwork
                          RoutingNetworkTest.FUNC_SetupHybridNetwork
                          RoutingNetworkTest.FUNC_Send
                          RoutingNetworkTest.FUNC_SendToNonExistingNode
                          RoutingNetworkTest.FUNC_ClientSend
                          RoutingNetworkTest.FUNC_SendMulti
                          RoutingNetworkTest.FUNC_ClientSendMulti
                          RoutingNetworkTest.FUNC_SendToGroup
                          RoutingNetworkTest.FUNC_SendToGroupSelfId
                          RoutingNetworkTest.FUNC_SendToGroupClientSelfId
                          RoutingNetworkTest.FUNC_SendToGroupInHybridNetwork
                          RoutingNetworkTest.FUNC_SendToGroupRandomId
                          RoutingNetworkTest.FUNC_AnonymousSendToGroupRandomId
                          RoutingNetworkTest.FUNC_AnonymousSendToGroupExistingId
                          RoutingNetworkTest.FUNC_JoinAfterBootstrapLeaves
                          RoutingNetworkTest.FUNC_RecursiveCall
                          RoutingNetworkTest.FUNC_JoinWithSameId
                          RoutingNetworkTest.FUNC_SendToClientsWithSameId
                          RoutingNetworkTest.FUNC_SendToClientWithSameId
                          RoutingNetworkTest.FUNC_NodeRemoved
                          RoutingNetworkTest.FUNC_GetRandomExistingNode
                          RoutingNetworkTest.FUNC_BasicNetworkChurn
                          RoutingNetworkTest.FUNC_MessagingNetworkChurn
                          FindNodeNetwork.FUNC_FindExistingNode
                          FindNodeNetwork.FUNC_FindExistingNodeInHybridNetwork
                          FindNodeNetwork.FUNC_FindNonExistingNode
                          FindNodeNetwork.FUNC_FindNodeAfterDrop
                          FindNodeNetwork.FUNC_VaultFindVaultNode
                          FindNodeNetwork.FUNC_VaultFindClientNode
                          FindNodeNetwork.FUNC_ClientFindVaultNode
                          FindNodeNetwork.FUNC_ClientFindClientNode
                          APITest.BEH_API_ZeroState
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
