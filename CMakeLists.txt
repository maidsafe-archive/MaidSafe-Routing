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
set(RoutingTestsHelperFiles ${RoutingSourcesDir}/tests/routing_network.cc
                            ${PROJECT_SOURCE_DIR}/include/maidsafe/routing/tests/routing_network.h
                            ${RoutingSourcesDir}/tests/test_utils.cc
                            ${RoutingSourcesDir}/tests/test_utils.h)
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
ms_add_static_library(maidsafe_routing ${RoutingAllFiles})
target_include_directories(maidsafe_routing PUBLIC ${PROJECT_SOURCE_DIR}/include PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(maidsafe_routing maidsafe_rudp maidsafe_passport maidsafe_network_viewer gmock gtest)

if(MaidsafeTesting)
  ms_add_static_library(maidsafe_routing_test_helper ${RoutingTestsHelperFiles})
  target_link_libraries(maidsafe_routing_test_helper maidsafe_routing)
  target_link_libraries(maidsafe_routing maidsafe_network_viewer)
  ms_add_executable(TESTrouting "Tests/Routing" ${RoutingTestsAllFiles})
  ms_add_executable(TESTrouting_api "Tests/Routing" ${RoutingApiTestFiles} ${RoutingSourcesDir}/tests/test_main.cc)
  # new executable TESTrouting_func is created to contain func tests excluded from TESTrouting, can be run seperately
  ms_add_executable(TESTrouting_func "Tests/Routing" ${RoutingFuncTestFiles})
  ms_add_executable(TESTrouting_func_nat "Tests/Routing" ${RoutingFuncNatTestFiles})
  # new executable TESTrouting_big is created to contain tests that each need their own network
  ms_add_executable(TESTrouting_big "Tests/Routing" ${RoutingBigTestFiles} ${RoutingSourcesDir}/tests/test_main.cc)
  ms_add_executable(create_client_bootstrap "Tools/Routing" ${RoutingSourcesDir}/tools/create_bootstrap.cc)
  ms_add_executable(routing_key_helper "Tools/Routing" ${RoutingSourcesDir}/tools/key_helper.cc)
  ms_add_executable(routing_node "Tools/Routing" ${RoutingSourcesDir}/tools/routing_node.cc
                                                 ${RoutingSourcesDir}/tools/commands.h
                                                 ${RoutingSourcesDir}/tools/commands.cc
                                                 ${RoutingSourcesDir}/tools/shared_response.h
                                                 ${RoutingSourcesDir}/tools/shared_response.cc)

  target_include_directories(maidsafe_routing_test_helper PRIVATE ${PROJECT_SOURCE_DIR}/src)
  target_include_directories(TESTrouting PRIVATE ${PROJECT_SOURCE_DIR}/src)
  target_include_directories(TESTrouting_api PRIVATE ${PROJECT_SOURCE_DIR}/src)
  target_include_directories(TESTrouting_func PRIVATE ${PROJECT_SOURCE_DIR}/src)
  target_include_directories(TESTrouting_func_nat PRIVATE ${PROJECT_SOURCE_DIR}/src)
  target_include_directories(TESTrouting_big PRIVATE ${PROJECT_SOURCE_DIR}/src)
  target_include_directories(routing_key_helper PRIVATE ${PROJECT_SOURCE_DIR}/src)
  target_include_directories(routing_node PRIVATE ${PROJECT_SOURCE_DIR}/src)

  target_link_libraries(TESTrouting maidsafe_routing_test_helper)
  target_link_libraries(TESTrouting_api maidsafe_routing_test_helper)
  target_link_libraries(TESTrouting_func maidsafe_routing_test_helper)
  target_link_libraries(TESTrouting_func_nat maidsafe_routing_test_helper)
  target_link_libraries(TESTrouting_big maidsafe_routing_test_helper)
  target_link_libraries(create_client_bootstrap maidsafe_routing_test_helper)
  target_link_libraries(routing_key_helper maidsafe_routing_test_helper)
  target_link_libraries(routing_node maidsafe_routing_test_helper)

  foreach(Target maidsafe_routing TESTrouting_func TESTrouting_func_nat TESTrouting_big routing_node maidsafe_routing_test_helper)
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
  ms_add_style_test()
  add_test(NAME Multiple_Functional_Tests COMMAND TESTrouting_func)
  set_property(TEST Multiple_Functional_Tests PROPERTY LABELS Routing Functional)
  set_property(TEST Multiple_Functional_Tests PROPERTY TIMEOUT 600)
  ms_add_gtests(TESTrouting)
  ms_add_gtests(TESTrouting_api)
# This test target can be run separately. Removed from Experimental target to reduce the ctest time.
  if(WEEKLY)
    ms_add_gtests(TESTrouting_big)
  endif()
  ms_add_project_experimental()
  ms_test_summary_output()
endif()
