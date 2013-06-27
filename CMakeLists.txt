#==================================================================================================#
# Copyright 2012 MaidSafe.net limited                                                              #
#                                                                                                  #
# This MaidSafe Software is licensed under the MaidSafe.net Commercial License, version 1.0 or     #
# later, and The General Public License (GPL), version 3. By contributing code to this project You #
# agree to the terms laid out in the MaidSafe Contributor Agreement, version 1.0, found in the     #
# root directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also         #
# available at:                                                                                    #
#                                                                                                  #
# http://www.novinet.com/license                                                                   #
#                                                                                                  #
# Unless required by applicable law or agreed to in writing, software distributed under the        #
# License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either #
# express or implied. See the License for the specific language governing permissions and          #
# limitations under the License.                                                                   #
#                                                                                                  #
#==================================================================================================#


set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

cmake_minimum_required(VERSION 2.8.10 FATAL_ERROR)
project(routing)

include(../../cmake_modules/standard_setup.cmake)

include_directories(${common_SOURCE_DIR}/include)
include_directories(${passport_SOURCE_DIR}/include)
include_directories(${private_SOURCE_DIR}/include)
include_directories(${rudp_SOURCE_DIR}/include)


#==================================================================================================#
# Set up all files as GLOBs                                                                        #
#==================================================================================================#
set(RoutingSourcesDir ${PROJECT_SOURCE_DIR}/src/maidsafe/routing)
glob_dir(Routing ${RoutingSourcesDir} Routing)
glob_dir(RoutingTests ${RoutingSourcesDir}/tests Tests)
glob_dir(RoutingTools ${RoutingSourcesDir}/tools Tools)
set(RoutingTestsHelperFiles ${RoutingSourcesDir}/tests/routing_network.cc
                            ${PROJECT_SOURCE_DIR}/include/maidsafe/routing/tests/routing_network.h
                            ${RoutingSourcesDir}/tests/test_utils.cc
                            ${RoutingSourcesDir}/tests/test_utils.h)
# TODO - RoutingTestsHelperFiles should probably be made into a new library.
set(RoutingAllFiles ${RoutingAllFiles} ${RoutingTestsHelperFiles})
set(RoutingApiTestFiles ${RoutingSourcesDir}/tests/routing_api_test.cc)
set(RoutingFuncTestFiles ${RoutingSourcesDir}/tests/routing_functional_test.cc
                         ${RoutingSourcesDir}/tests/routing_functional_non_nat_test.cc
                         ${RoutingSourcesDir}/tests/test_func_main.cc)
set(RoutingFuncNatTestFiles ${RoutingSourcesDir}/tests/routing_functional_test.cc
                            ${RoutingSourcesDir}/tests/routing_functional_nat_test.cc
                            ${RoutingSourcesDir}/tests/test_func_nat_main.cc)
set(RoutingBigTestFiles ${RoutingSourcesDir}/tests/routing_churn_test.cc
                        ${RoutingSourcesDir}/tests/find_nodes_test.cc
                        ${RoutingSourcesDir}/tests/routing_stand_alone_test.cc)

list(REMOVE_ITEM RoutingTestsAllFiles ${RoutingTestsHelperFiles}
                                      ${RoutingApiTestFiles}
                                      ${RoutingFuncTestFiles}
                                      ${RoutingFuncNatTestFiles}
                                      ${RoutingBigTestFiles})


#==================================================================================================#
# Define MaidSafe libraries and executables                                                        #
#==================================================================================================#
ms_add_static_library(routing ${RoutingAllFiles})
target_link_libraries(maidsafe_routing maidsafe_rudp maidsafe_passport maidsafe_network_viewer)

if(MaidsafeTesting)
  target_link_libraries(maidsafe_routing maidsafe_network_viewer)
  ms_add_executable(TESTrouting "Tests/Routing" ${RoutingTestsAllFiles})
  ms_add_executable(TESTrouting_api "Tests/Routing" ${RoutingApiTestFiles} ${RoutingSourcesDir}/tests/test_main.cc)
  # new executable TESTrouting_func is created to contain func tests excluded from TESTrouting, can be run seperately
  ms_add_executable(TESTrouting_func "Tests/Routing" ${RoutingFuncTestFiles})
  ms_add_executable(TESTrouting_func_nat "Tests/Routing" ${RoutingFuncNatTestFiles})
  # new executable TESTrouting_big is created to contain tests that each need their own network
  ms_add_executable(TESTrouting_big "Tests/Routing" ${RoutingBigTestFiles} ${RoutingSourcesDir}/tests/test_main.cc)
  ms_add_executable(create_lifestuff_bootstrap "Tools" ${RoutingSourcesDir}/tools/create_bootstrap.cc)
  ms_add_executable(routing_key_helper "Tools" ${RoutingSourcesDir}/tools/key_helper.cc)
  ms_add_executable(routing_node "Tools" ${RoutingSourcesDir}/tools/routing_node.cc
                                         ${RoutingSourcesDir}/tools/commands.h
                                         ${RoutingSourcesDir}/tools/commands.cc
                                         ${RoutingSourcesDir}/tools/shared_response.h
                                         ${RoutingSourcesDir}/tools/shared_response.cc)
  target_link_libraries(TESTrouting maidsafe_routing)
  target_link_libraries(TESTrouting_api maidsafe_routing)
  target_link_libraries(TESTrouting_func maidsafe_routing)
  target_link_libraries(TESTrouting_func_nat maidsafe_routing)
  target_link_libraries(TESTrouting_big maidsafe_routing)
  target_link_libraries(create_lifestuff_bootstrap maidsafe_routing)
  target_link_libraries(routing_key_helper maidsafe_routing)
  target_link_libraries(routing_node maidsafe_routing)
endif()

rename_outdated_built_exes()


#==================================================================================================#
# Set compiler and linker flags                                                                    #
#==================================================================================================#
include(standard_flags)

if(QA_BUILD)
  add_definitions(-DQA_BUILD)
endif()


#==================================================================================================#
# Tests                                                                                            #
#==================================================================================================#
if(MaidsafeTesting)
  add_style_test()
  add_test(NAME Multiple_Functional_Tests COMMAND TESTrouting_func)
  set_property(TEST Multiple_Functional_Tests PROPERTY LABELS Routing Functional)
  set_property(TEST Multiple_Functional_Tests PROPERTY TIMEOUT 600)
  add_gtests(TESTrouting_big)
  add_gtests(TESTrouting)
  add_gtests(TESTrouting_api)
  label_as_critical_tests(APITest.BEH_API_ZeroState
                          APITest.BEH_API_SendToSelf
                          APITest.BEH_API_ClientNode
                          APITest.BEH_API_NonMutatingClientNode
                          APITest.BEH_API_ClientNodeSameId
                          APITest.BEH_API_NodeNetwork
                          APITest.BEH_API_NodeNetworkWithClient
                          )
  add_project_experimental()
  test_summary_output()
endif()
