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
set(RoutingLibApiFiles
    ${PROJECT_SOURCE_DIR}/include/maidsafe/routing/close_nodes_change.h
    ${PROJECT_SOURCE_DIR}/include/maidsafe/routing/routing.h
    ${PROJECT_SOURCE_DIR}/include/maidsafe/routing/node_info.h
    ${PROJECT_SOURCE_DIR}/include/maidsafe/routing/types.h
    )
source_group("Routing API Files" FILES ${RoutingLibApiFiles})
set(RoutingLibHeaders
    ${RoutingSourcesDir}/header.h
    ${RoutingSourcesDir}/messages.h
    ${RoutingSourcesDir}/message_handler.h
    ${RoutingSourcesDir}/routing_table.h
    ${RoutingSourcesDir}/utils.h
    )
source_group("Routing Header Files" FILES ${RoutingLibHeaders})
set(RoutingLibSources
    # ${RoutingSourcesDir}/message_handler.cc
    ${RoutingSourcesDir}/routing_table.cc
    ${RoutingSourcesDir}/utils.cc
    )
source_group("Routing Source Files" FILES ${RoutingLibSources})
ms_add_static_library(maidsafe_routing  ${RoutingLibApiFiles} ${RoutingLibHeaders} ${RoutingLibSources})

target_include_directories(maidsafe_routing PUBLIC ${PROJECT_SOURCE_DIR}/include PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(maidsafe_routing maidsafe_rudp maidsafe_passport)

if(MaidsafeTesting)
  file(GLOB TestFiles ${RoutingSourcesDir}/tests/*.cc)
  foreach(TestFile ${TestFiles})
    get_filename_component(TestName ${TestFile} NAME_WE)
    string(REGEX REPLACE "(.*)_test$" "test_\\1" TestName ${TestName})
    ms_add_executable(${TestName} "Tests/Routing"
        ${TestFile}
        ${RoutingSourcesDir}/tests/main/test_main.cc
        ${RoutingSourcesDir}/tests/main/test_utils.h
        ${RoutingSourcesDir}/tests/main/test_utils.cc
        )

    target_include_directories(${TestName} PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(${TestName} maidsafe_routing maidsafe_test)
    list(APPEND RoutingTests ${TestName})
  endforeach()

  # TODO - remove these targets - only added to avoid changing installers for now.
  ms_add_executable(test_routing "Tests/Routing" ${RoutingSourcesDir}/tests/main/test_main.cc)
  ms_add_executable(test_routing_api "Tests/Routing" ${RoutingSourcesDir}/tests/main/test_main.cc)
  target_include_directories(test_routing PRIVATE ${PROJECT_SOURCE_DIR}/src)
  target_include_directories(test_routing_api PRIVATE ${PROJECT_SOURCE_DIR}/src)
  target_link_libraries(test_routing maidsafe_routing maidsafe_test)
  target_link_libraries(test_routing_api maidsafe_routing maidsafe_test)
endif()

ms_rename_outdated_built_exes()


#==================================================================================================#
# Set compiler and linker flags                                                                    #
#==================================================================================================#
include(standard_flags)


#==================================================================================================#
# Tests                                                                                            #
#==================================================================================================#
if(MaidsafeTesting)
  ms_add_default_tests()
  foreach(RoutingTest ${RoutingTests})
    ms_add_gtests(${RoutingTest})
  endforeach()
  ms_test_summary_output()
endif()
