#==================================================================================================#
#                                                                                                  #
#  Copyright 2012 MaidSafe.net limited                                                             #
#                                                                                                  #
#  This MaidSafe Software is licensed to you under (1) the MaidSafe.net Commercial License,        #
#  version 1.0 or later, or (2) The General Public License (GPL), version 3, depending on which    #
#  licence you accepted on initial access to the Software (the "Licences").                        #
#                                                                                                  #
#  By contributing code to the MaidSafe Software, or to this project generally, you agree to be    #
#  bound by the terms of the MaidSafe Contributor Agreement, version 1.0, found in the root        #
#  directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also available   #
#  at: http://www.maidsafe.net/licenses                                                            #
#                                                                                                  #
#  Unless required by applicable law or agreed to in writing, the MaidSafe Software distributed    #
#  under the GPL Licence is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF   #
#  ANY KIND, either express or implied.                                                            #
#                                                                                                  #
#  See the Licences for the specific language governing permissions and limitations relating to    #
#  use of the MaidSafe Software.                                                                   #
#                                                                                                  #
#==================================================================================================#


set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/standard_setup.cmake")
  cmake_minimum_required(VERSION 2.8)  # To suppress warning cluttering error message
  set(Msg "\nThis project can currently only be build as part of the MaidSafe super-project.  For")
  set(Msg "${Msg} full details, see https://github.com/maidsafe/MaidSafe/wiki/Build-Instructions\n")
  message(FATAL_ERROR "${Msg}")
endif()

project(routing)

include(../../cmake_modules/standard_setup.cmake)


#==================================================================================================#
# Set up all files as GLOBs                                                                        #
#==================================================================================================#
set(RoutingSourcesDir ${PROJECT_SOURCE_DIR}/src/maidsafe/routing)
ms_glob_dir(Routing ${RoutingSourcesDir} Routing)
ms_glob_dir(RoutingTests ${RoutingSourcesDir}/tests Tests)
ms_glob_dir(RoutingTools ${RoutingSourcesDir}/tools Tools)
set(RoutingTestsHelperFiles
                            ${RoutingSourcesDir}/tests/zero_state_helpers.cc
                            ${PROJECT_SOURCE_DIR}/include/maidsafe/routing/tests/zero_state_helpers.h
                            ${RoutingSourcesDir}/tests/test_utils.cc
                            ${RoutingSourcesDir}/tests/test_utils.h)
set(RoutingApiTestFiles ${RoutingSourcesDir}/tests/routing_api_test.cc)
set(RoutingFuncTestFiles ${RoutingSourcesDir}/tests/routing_functional_test.cc
                         ${RoutingSourcesDir}/tests/test_func_main.cc)
set(RoutingBigTestFiles ${RoutingSourcesDir}/tests/cache_test.cc
                        ${RoutingSourcesDir}/tests/routing_churn_test.cc
                        ${RoutingSourcesDir}/tests/find_nodes_test.cc
                        ${RoutingSourcesDir}/tests/routing_stand_alone_test.cc)

list(REMOVE_ITEM RoutingTestsAllFiles ${RoutingTestsHelperFiles}
                                      ${RoutingApiTestFiles}
                                      ${RoutingFuncTestFiles}
                                      ${RoutingBigTestFiles})


#==================================================================================================#
# Define MaidSafe libraries and executables                                                        #
#==================================================================================================#
ms_add_static_library(maidsafe_routing ${RoutingAllFiles})
target_include_directories(maidsafe_routing PUBLIC ${PROJECT_SOURCE_DIR}/include PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(maidsafe_routing maidsafe_rudp maidsafe_passport maidsafe_network_viewer)

if(MaidsafeTesting)
  ms_add_static_library(maidsafe_routing_test_helper ${RoutingTestsHelperFiles})
  target_link_libraries(maidsafe_routing_test_helper maidsafe_routing maidsafe_test)
  target_link_libraries(maidsafe_routing maidsafe_network_viewer)
  ms_add_executable(test_routing "Tests/Routing" ${RoutingTestsAllFiles})
  ms_add_executable(test_routing_api "Tests/Routing" ${RoutingApiTestFiles} ${RoutingSourcesDir}/tests/test_main.cc)
  # new executable test_routing_func is created to contain func tests excluded from test_routing, can be run seperately
  ms_add_executable(test_routing_func "Tests/Routing" ${RoutingFuncTestFiles})
  # new executable weekly_test_routing is created to contain tests that each need their own network
  ms_add_executable(weekly_test_routing "Tests/Routing" ${RoutingBigTestFiles} ${RoutingSourcesDir}/tests/test_main.cc)
  ms_add_executable(create_client_bootstrap "Tools/Routing" ${RoutingSourcesDir}/tools/create_bootstrap.cc)
  ms_add_executable(routing_key_helper "Tools/Routing" ${RoutingSourcesDir}/tools/key_helper.cc)
  ms_add_executable(routing_node "Tools/Routing" ${RoutingSourcesDir}/tools/routing_node.cc
                                                 ${RoutingSourcesDir}/tools/commands.h
                                                 ${RoutingSourcesDir}/tools/commands.cc
                                                 ${RoutingSourcesDir}/tools/shared_response.h
                                                 ${RoutingSourcesDir}/tools/shared_response.cc)

  target_include_directories(maidsafe_routing_test_helper PRIVATE ${PROJECT_SOURCE_DIR}/src)
  target_include_directories(test_routing PRIVATE ${PROJECT_SOURCE_DIR}/src)
  target_include_directories(test_routing_api PRIVATE ${PROJECT_SOURCE_DIR}/src)
  target_include_directories(test_routing_func PRIVATE ${PROJECT_SOURCE_DIR}/src)
  target_include_directories(weekly_test_routing PRIVATE ${PROJECT_SOURCE_DIR}/src)
  target_include_directories(routing_key_helper PRIVATE ${PROJECT_SOURCE_DIR}/src)
  target_include_directories(routing_node PRIVATE ${PROJECT_SOURCE_DIR}/src)
  target_include_directories(create_client_bootstrap PRIVATE ${PROJECT_SOURCE_DIR}/src)

  target_link_libraries(test_routing maidsafe_routing_test_helper)
  target_link_libraries(test_routing_api maidsafe_routing_test_helper)
  target_link_libraries(test_routing_func maidsafe_routing_test_helper)
  target_link_libraries(weekly_test_routing maidsafe_routing_test_helper)
  target_link_libraries(create_client_bootstrap maidsafe_routing_test_helper)
  target_link_libraries(routing_key_helper maidsafe_routing_test_helper)
  target_link_libraries(routing_node maidsafe_routing_test_helper)
  foreach(Target maidsafe_routing test_routing_func weekly_test_routing routing_node maidsafe_routing_test_helper)
    target_compile_definitions(${Target} PRIVATE USE_GTEST)
  endforeach()
endif()

ms_rename_outdated_built_exes()

add_subdirectory(${RoutingSourcesDir}/tools/network_viewer)


#==================================================================================================#
# Set compiler and linker flags                                                                    #
#==================================================================================================#
include(standard_flags)

target_compile_definitions(maidsafe_routing PRIVATE $<$<BOOL:${QA_BUILD}>:QA_BUILD>)


#==================================================================================================#
# Tests                                                                                            #
#==================================================================================================#
if(MaidsafeTesting)
  ms_add_default_tests()
  add_test(NAME Multiple_Functional_Tests COMMAND test_routing_func)
  set_property(TEST Multiple_Functional_Tests PROPERTY LABELS Routing Functional)
  ms_add_gtests(test_routing)
  ms_add_gtests(test_routing_api)
  set(Timeout 300)
  ms_update_test_timeout(Timeout)
  set_property(TEST CloseNodesChangeTest.BEH_FullSizeRoutingTable PROPERTY TIMEOUT ${Timeout})
  set_property(TEST ResponseHandlerTest.BEH_FindNodes PROPERTY TIMEOUT ${Timeout})
  set_property(TEST APITest.BEH_API_SendGroup PROPERTY TIMEOUT ${Timeout})
  set_property(TEST APITest.BEH_API_TypedMessagePartiallyJoinedSendReceive PROPERTY TIMEOUT ${Timeout})
  set_property(TEST APITest.BEH_API_NodeNetworkWithClient PROPERTY TIMEOUT ${Timeout})
# This test target can be run separately. Removed from Experimental target to reduce the ctest time.
  if(WEEKLY)
    ms_add_gtests(weekly_test_routing)
  endif()
  ms_test_summary_output()
endif()


#==================================================================================================#
# Package                                                                                          #
#==================================================================================================#
install(TARGETS maidsafe_routing COMPONENT Development CONFIGURATIONS Debug Release ARCHIVE DESTINATION lib)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/ COMPONENT Development DESTINATION include)

if(MaidsafeTesting)
  install(TARGETS maidsafe_routing_test_helper test_routing test_routing_api test_routing_func
                  weekly_test_routing create_client_bootstrap routing_key_helper routing_node
                  COMPONENT Tests CONFIGURATIONS Debug RUNTIME DESTINATION bin/debug ARCHIVE DESTINATION lib)
  install(TARGETS maidsafe_routing_test_helper test_routing test_routing_api test_routing_func
                  weekly_test_routing create_client_bootstrap routing_key_helper routing_node
                  COMPONENT Tests CONFIGURATIONS Release RUNTIME DESTINATION bin ARCHIVE DESTINATION lib)
endif()
