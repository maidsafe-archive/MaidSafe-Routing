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
# Define MaidSafe libraries and executables                                                        #
#==================================================================================================#
set(RoutingSourcesDir ${PROJECT_SOURCE_DIR}/src/maidsafe/routing)
ms_add_static_library(maidsafe_routing 
                     ${RoutingSourcesDir}/routing_table.cc
                     ${RoutingSourcesDir}/utils.cc
                     ${RoutingSourcesDir}/node_info.cc
                     
                     )
target_include_directories(maidsafe_routing PUBLIC ${PROJECT_SOURCE_DIR}/include PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(maidsafe_routing maidsafe_rudp maidsafe_passport)

ms_add_executable(test_routing_table "Tests/Routing"
                 ${RoutingSourcesDir}/tests/routing_table_test.cc
                 ${RoutingSourcesDir}/tests/test_main.cc
                 ${RoutingSourcesDir}/tests/test_utils.cc
                 )
target_include_directories(test_routing_table PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(test_routing_table maidsafe_routing gtest)

ms_rename_outdated_built_exes()



#==================================================================================================#
# Set compiler and linker flags                                                                    #
#==================================================================================================#
include(standard_flags)

target_compile_definitions(maidsafe_routing PRIVATE $<$<BOOL:${QA_BUILD}>:QA_BUILD>)


#==================================================================================================#
# Tests                                                                                            #
#==================================================================================================#
  ms_add_gtests(test_routing_table)
