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
    ${PROJECT_SOURCE_DIR}/include/maidsafe/routing/client.h
    ${PROJECT_SOURCE_DIR}/include/maidsafe/routing/node_info.h
    ${PROJECT_SOURCE_DIR}/include/maidsafe/routing/routing_node.h
    ${PROJECT_SOURCE_DIR}/include/maidsafe/routing/types.h
    )
source_group("Routing API Files" FILES ${RoutingLibApiFiles})
set(RoutingLibHeaders
    ${RoutingSourcesDir}/accumulator.h
    ${RoutingSourcesDir}/bootstrap_handler.h
    ${RoutingSourcesDir}/connection_manager.h
    ${RoutingSourcesDir}/message_header.h
    ${RoutingSourcesDir}/routing_table.h
    ${RoutingSourcesDir}/sentinel.h
    ${RoutingSourcesDir}/utils.h
    )
source_group("Routing Header Files" FILES ${RoutingLibHeaders})
set(RoutingMessagesHeaders
    ${RoutingSourcesDir}/messages/connect.h
    ${RoutingSourcesDir}/messages/connect_response.h
    ${RoutingSourcesDir}/messages/find_group.h
    ${RoutingSourcesDir}/messages/find_group_response.h
    ${RoutingSourcesDir}/messages/get_data.h
    ${RoutingSourcesDir}/messages/get_data_response.h
    ${RoutingSourcesDir}/messages/messages.h
    ${RoutingSourcesDir}/messages/messages_fwd.h
    ${RoutingSourcesDir}/messages/post_message.h
    ${RoutingSourcesDir}/messages/put_data.h
    ${RoutingSourcesDir}/messages/put_data_response.h
    )
source_group("Routing Messages Header Files" FILES ${RoutingMessagesHeaders})
set(RoutingLibSources
    ${RoutingSourcesDir}/bootstrap_handler.cc
    ${RoutingSourcesDir}/client.cc
    ${RoutingSourcesDir}/connection_manager.cc
    ${RoutingSourcesDir}/routing_table.cc
    ${RoutingSourcesDir}/routing_node.cc
    ${RoutingSourcesDir}/sentinel.cc
    )
source_group("Routing Source Files" FILES ${RoutingLibSources})
ms_add_static_library(maidsafe_routing
    ${RoutingLibApiFiles}
    ${RoutingLibHeaders}
    ${RoutingMessagesHeaders}
    ${RoutingLibSources}
    )

target_include_directories(maidsafe_routing PUBLIC ${PROJECT_SOURCE_DIR}/include PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(maidsafe_routing maidsafe_rudp maidsafe_passport ${BoostCoroutineLibs} ${BoostContextLibs})

if(INCLUDE_TESTS)
  ms_add_static_library(maidsafe_test_routing
        ${RoutingSourcesDir}/tests/utils/test_main.cc
        ${RoutingSourcesDir}/tests/utils/test_utils.h
        ${RoutingSourcesDir}/tests/utils/test_utils.cc
        ${RoutingSourcesDir}/tests/utils/routing_table_unit_test.h
        ${RoutingSourcesDir}/tests/utils/routing_table_unit_test.cc
        )
  target_include_directories(maidsafe_test_routing PUBLIC ${PROJECT_SOURCE_DIR}/src)
  target_link_libraries(maidsafe_test_routing maidsafe_routing maidsafe_test)

  file(GLOB TestFiles ${RoutingSourcesDir}/tests/*.cc)
  file(GLOB MessagesTestFiles ${RoutingSourcesDir}/messages/tests/*.cc)
  file(GLOB MessagesTestFileHeaders ${RoutingSourcesDir}/messages/tests/*.h)
  foreach(TestFile ${TestFiles}  ${MessagesTestFiles})
    get_filename_component(TestName ${TestFile} NAME_WE)
    ms_add_executable(${TestName} "Tests/Routing" ${TestFile})
    target_link_libraries(${TestName} maidsafe_test_routing)
    list(APPEND RoutingTests ${TestName})
  endforeach()

  # TODO - remove these targets - only added to avoid changing installers for now.
  ms_add_executable(test_routing "Tests/Routing" ${RoutingSourcesDir}/tests/utils/test_main.cc)
  ms_add_executable(test_routing_api "Tests/Routing" ${RoutingSourcesDir}/tests/utils/test_main.cc)
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
function(ms_add_test TestTarget)
  add_test(NAME ${TestTarget} COMMAND ${TestTarget})
  set(AllGtests ${AllGtests} ${TestTarget} PARENT_SCOPE)
  set_property(TEST ${TestTarget} PROPERTY LABELS ${CamelCaseProjectName} Behavioural ${NetworkTestLabel} ${TASK_LABEL})
endfunction()

if(INCLUDE_TESTS)
  enable_testing()
  foreach(RoutingTest ${RoutingTests})
    ms_add_test("${RoutingTest}")
  endforeach()
  ms_add_default_tests()
  ms_test_summary_output()
endif()
