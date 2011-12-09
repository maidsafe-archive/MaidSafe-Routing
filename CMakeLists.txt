#==============================================================================#
#                                                                              #
#  Copyright (c) 2011 MaidSafe.net limited                                     #
#  All rights reserved.                                                        #
#                                                                              #
#  Redistribution and use in source and binary forms, with or without          #
#  modification, are permitted provided that the following conditions are met: #
#                                                                              #
#      * Redistributions of source code must retain the above copyright        #
#        notice, this list of conditions and the following disclaimer.         #
#      * Redistributions in binary form must reproduce the above copyright     #
#        notice, this list of conditions and the following disclaimer in the   #
#        documentation and/or other materials provided with the distribution.  #
#      * Neither the name of the maidsafe.net limited nor the names of its     #
#        contributors may be used to endorse or promote products derived from  #
#        this software without specific prior written permission.              #
#                                                                              #
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" #
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE   #
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE  #
#  ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE  #
#  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR         #
#  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF        #
#  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS    #
#  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN     #
#  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)     #
#  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE  #
#  POSSIBILITY OF SUCH DAMAGE.                                                 #
#                                                                              #
#==============================================================================#
#                                                                              #
#  Written by maidsafe.net team                                                #
#                                                                              #
#==============================================================================#


SET(MS_PROJECT_NAME maidsafe_dht)

CMAKE_MINIMUM_REQUIRED(VERSION 2.8.4 FATAL_ERROR)
MESSAGE("================================================================================\n")

GET_FILENAME_COMPONENT(CMAKE_BUILD_TYPE_DIR ${CMAKE_BINARY_DIR} NAME)

# Variable MSVC is not set until after "PROJECT" command below
IF(CMAKE_BUILD_TYPE_DIR MATCHES "Win_MSVC")
  SET(CMAKE_BUILD_TYPE_DIR Release)
ELSEIF(CMAKE_BUILD_TYPE_DIR MATCHES "kdev")
  SET(CMAKE_BUILD_TYPE_DIR Debug)
  SET(KDEV 1)
ENDIF()

IF(NOT ${CMAKE_C_COMPILER_WORKS})
  MESSAGE(FATAL_ERROR "No generator previously specified.\nTo see a full list of generators, run:\n\tcmake --help\n")
ENDIF()

# This "PROJECT" command resets CMAKE_BUILD_TYPE, hence it's copied from CMAKE_BUILD_TYPE_DIR which is set above.
IF((CMAKE_BUILD_TYPE_DIR MATCHES "Release") OR (DEFINED ${KDEV}))
  PROJECT(${MS_PROJECT_NAME})
ELSE()
  STRING(TOLOWER ${CMAKE_BUILD_TYPE_DIR} MS_PROJECT_NAME_APPENDIX)
  PROJECT(${MS_PROJECT_NAME}_${MS_PROJECT_NAME_APPENDIX})
ENDIF()

SET(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE_DIR} CACHE INTERNAL "Except for MSVC, the build type is set by running cmake from the appropriate build subdirectory e.g. for a Debug build, \"cd build/Linux/Debug && cmake ../../..\"." FORCE)


###################################################################################################
# MaidSafe Common & Transport library search                                                      #
###################################################################################################
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/build)
INCLUDE(maidsafe_find_common)
ADD_MAIDSAFE_LIBRARY(MaidSafe-Transport)


###################################################################################################
# Standard setup                                                                                  #
###################################################################################################
SET(PROTO_SOURCE_DIR ${PROJECT_SOURCE_DIR}/src)
FILE(GLOB PROTO_FILES_KADEMLIA RELATIVE ${PROTO_SOURCE_DIR} "${PROTO_SOURCE_DIR}/maidsafe/dht/*.proto")
FILE(GLOB PROTO_FILES_TESTS RELATIVE ${PROTO_SOURCE_DIR} "${PROTO_SOURCE_DIR}/maidsafe/dht/tests/*.proto")
SET(PROTO_FILES "${PROTO_FILES_KADEMLIA};${PROTO_FILES_TESTS}")
INCLUDE(maidsafe_standard_setup)
HANDLE_VERSIONS(${PROJECT_SOURCE_DIR}/src/maidsafe/dht/version.h)


###################################################################################################
# Set up all files as GLOBs                                                                       #
###################################################################################################
####    CODE    ####
FILE(GLOB SOURCE_FILES_DHT_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/*.cc")
FILE(GLOB HEADER_FILES_DHT_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/*.h")
SET(ALL_FILES_DHT_DIR ${SOURCE_FILES_DHT_DIR} ${HEADER_FILES_DHT_DIR})
SET(STYLE_DHT ${ALL_FILES_DHT_DIR})
LIST(REMOVE_ITEM STYLE_DHT "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia.pb.cc")
LIST(REMOVE_ITEM STYLE_DHT "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia.pb.h")
LIST(REMOVE_ITEM STYLE_DHT "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/rpcs.pb.cc")
LIST(REMOVE_ITEM STYLE_DHT "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/rpcs.pb.h")
SOURCE_GROUP("DHT Source Files" FILES ${SOURCE_FILES_DHT_DIR})
SOURCE_GROUP("DHT Header Files" FILES ${HEADER_FILES_DHT_DIR})

####    BENCHMARK, DEMO & TESTS    ####
FILE(GLOB SOURCE_FILES_BENCHMARK_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/benchmark/*.cc")
FILE(GLOB HEADER_FILES_BENCHMARK_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/benchmark/*.h")
SET(ALL_FILES_BENCHMARK_DIR ${SOURCE_FILES_BENCHMARK_DIR} ${HEADER_FILES_BENCHMARK_DIR})
SOURCE_GROUP("Benchmark Source Files" FILES ${SOURCE_FILES_BENCHMARK_DIR})
SOURCE_GROUP("Benchmark Header Files" FILES ${HEADER_FILES_BENCHMARK_DIR})

FILE(GLOB SOURCE_FILES_DEMO_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/demo/*.cc")
FILE(GLOB HEADER_FILES_DEMO_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/demo/*.h")
SET(ALL_FILES_DEMO_DIR ${SOURCE_FILES_DEMO_DIR} ${HEADER_FILES_DEMO_DIR})
SOURCE_GROUP("Demo Source Files" FILES ${SOURCE_FILES_DEMO_DIR})
SOURCE_GROUP("Demo Header Files" FILES ${HEADER_FILES_DEMO_DIR})

FILE(GLOB SOURCE_FILES_DHT_TESTS_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/tests/*.cc")
FILE(GLOB HEADER_FILES_DHT_TESTS_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/tests/*.h")
SET(ALL_FILES_DHT_TESTS_DIR ${SOURCE_FILES_DHT_TESTS_DIR} ${HEADER_FILES_DHT_TESTS_DIR})
SET(STYLE_DHT_TESTS ${ALL_FILES_DHT_TESTS_DIR})
SOURCE_GROUP("DHT Tests Source Files" FILES ${SOURCE_FILES_DHT_TESTS_DIR})
SOURCE_GROUP("DHT Tests Header Files" FILES ${HEADER_FILES_DHT_TESTS_DIR})
LIST(REMOVE_ITEM STYLE_DHT_TESTS "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/tests/wrapper.pb.cc")
LIST(REMOVE_ITEM STYLE_DHT_TESTS "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/tests/wrapper.pb.h")

FILE(GLOB SOURCE_FILES_DHT_TESTS_FUNCTIONAL_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/tests/functional/*.cc")
FILE(GLOB HEADER_FILES_DHT_TESTS_FUNCTIONAL_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/tests/functional/*.h")
SET(ALL_FILES_DHT_TESTS_FUNCTIONAL_DIR ${SOURCE_FILES_DHT_TESTS_FUNCTIONAL_DIR} ${HEADER_FILES_DHT_TESTS_FUNCTIONAL_DIR})
SOURCE_GROUP("Functional Tests Source Files" FILES ${SOURCE_FILES_DHT_TESTS_FUNCTIONAL_DIR})
SOURCE_GROUP("Functional Tests Header Files" FILES ${HEADER_FILES_DHT_TESTS_FUNCTIONAL_DIR})

SET(SOURCE_FILES_DHT_TESTS_FUNCTIONAL_NODE "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/tests/functional/node_test.cc"
                                           "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/tests/test_utils.cc"
                                           "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/tests/test_main.cc")
SET(SOURCE_FILES_DHT_TESTS_FUNCTIONAL_NODE_IMPL "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/tests/functional/node_impl_test.cc"
                                                "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/tests/test_utils.cc"
                                                "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/tests/test_main.cc")
SET(SOURCE_FILES_DHT_TESTS_FUNCTIONAL_NODE_CHURN "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/tests/functional/node_churn_test.cc"
                                                 "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/tests/test_utils.cc"
                                                 "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/tests/test_main.cc")
SET(HEADER_FILES_DHT_TESTS_FUNCTIONALS "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/tests/functional/test_node_environment.h"
                                       "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/tests/test_utils.h"
                                       "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/tests/local_network.h")


###################################################################################################
# Define MaidSafe libraries and executables                                                       #
###################################################################################################
MS_ADD_STATIC_LIBRARY(${MS_PROJECT_NAME} ${ALL_FILES_DHT_DIR})
MS_ADD_EXECUTABLE(TESTkademlia Tests ${ALL_FILES_DHT_TESTS_DIR})
MS_ADD_EXECUTABLE(TESTnode_functional Tests ${SOURCE_FILES_DHT_TESTS_FUNCTIONAL_NODE} ${HEADER_FILES_DHT_TESTS_FUNCTIONALS})
MS_ADD_EXECUTABLE(TESTnode_impl_functional Tests ${SOURCE_FILES_DHT_TESTS_FUNCTIONAL_NODE_IMPL} ${HEADER_FILES_DHT_TESTS_FUNCTIONALS})
MS_ADD_EXECUTABLE(TESTnode_churn Tests ${SOURCE_FILES_DHT_TESTS_FUNCTIONAL_NODE_CHURN} ${HEADER_FILES_DHT_TESTS_FUNCTIONALS})
MS_ADD_EXECUTABLE(KademliaDemo Demo ${ALL_FILES_DEMO_DIR})
MS_ADD_EXECUTABLE(Benchmark Benchmark ${ALL_FILES_BENCHMARK_DIR})

TARGET_LINK_LIBRARIES(maidsafe_dht_static
                        maidsafe_transport_static
                        maidsafe_common_static
                        breakpad_static
                        boost_date_time
                        boost_regex)
TARGET_LINK_LIBRARIES(TESTkademlia maidsafe_dht_static gmock_static)
TARGET_LINK_LIBRARIES(TESTnode_functional maidsafe_dht_static gtest_static)
TARGET_LINK_LIBRARIES(TESTnode_impl_functional maidsafe_dht_static gtest_static)
TARGET_LINK_LIBRARIES(TESTnode_churn maidsafe_dht_static gtest_static)
TARGET_LINK_LIBRARIES(KademliaDemo maidsafe_dht_static boost_program_options)
TARGET_LINK_LIBRARIES(Benchmark maidsafe_dht_static boost_program_options)


###################################################################################################
# Set compiler and linker flags                                                                   #
###################################################################################################
INCLUDE(maidsafe_standard_flags)

SET_TARGET_PROPERTIES(TESTnode_functional PROPERTIES COMPILE_DEFINITIONS "FUNCTIONAL_NODE_TEST")
SET_TARGET_PROPERTIES(TESTnode_impl_functional PROPERTIES COMPILE_DEFINITIONS "FUNCTIONAL_NODE_IMPL_TEST")
SET_TARGET_PROPERTIES(TESTnode_churn PROPERTIES COMPILE_DEFINITIONS "FUNCTIONAL_CHURN_TEST")

IF(MSVC)
  # Warnings switched off for protocol buffer generated files
  SET_SOURCE_FILES_PROPERTIES(${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia.pb.cc
                              ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/rpcs.pb.cc
                              ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/tests/wrapper.pb.cc
                                PROPERTIES COMPILE_FLAGS "/W0")
  SET_TARGET_PROPERTIES(maidsafe_dht_static TESTkademlia PROPERTIES COMPILE_FLAGS "/bigobj")
  SET_TARGET_PROPERTIES(maidsafe_dht_static PROPERTIES STATIC_LIBRARY_FLAGS_RELEASE "/LTCG")
ELSEIF(UNIX)
  SET_TARGET_PROPERTIES(KademliaDemo PROPERTIES COMPILE_FLAGS "-g")
  FILE(GLOB PROTO_SOURCE_FILES "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/*.pb.cc"
                               "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/tests/*.pb.cc")
  SET_SOURCE_FILES_PROPERTIES(${PROJECT_SOURCE_DIR}/src/maidsafe/dht/service.cc
                              ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/node.cc
                              ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/node_impl.cc
                              ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/benchmark/benchmark.cc
                              ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/tests/test_main.cc
                              ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/tests/mock_rpcs_test.cc
                              ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/tests/mock_node_impl_test.cc
                              ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/tests/rpcs_test.cc
                              ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/tests/services_test.cc
                              ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/tests/functional/node_churn_test
                              ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/tests/functional/node_test.cc
                              ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/tests/functional/node_impl_test.cc
                              PROPERTIES COMPILE_FLAGS "-Wno-effc++")
  SET_SOURCE_FILES_PROPERTIES(${PROTO_SOURCE_FILES} PROPERTIES COMPILE_FLAGS "-w")
ENDIF()


###################################################################################################
# Tests                                                                                           #
###################################################################################################
ADD_STYLE_TEST(${STYLE_DHT}
               ${ALL_FILES_BENCHMARK_DIR}
               ${ALL_FILES_DEMO_DIR}
               ${STYLE_DHT_TESTS}
               ${ALL_FILES_DHT_TESTS_FUNCTIONAL_DIR})

ADD_GTESTS(TESTkademlia)
ADD_GTESTS(TESTnode_functional)
ADD_GTESTS(TESTnode_impl_functional)
ADD_GTESTS(TESTnode_churn)

TEST_SUMMARY_OUTPUT()

# Remove most lengthy tests and style checkers from MemCheck runs.
ADD_MEMCHECK_IGNORE(STYLE_CHECK)
ADD_MEMCHECK_IGNORE(NodeTest.FUNC_Ping)
ADD_MEMCHECK_IGNORE(NodeTest.FUNC_Bootstrap)
ADD_MEMCHECK_IGNORE(NodeTest.FUNC_JoinClient)
ADD_MEMCHECK_IGNORE(NodeTest.FUNC_StoreAndFindSmallValue)
ADD_MEMCHECK_IGNORE(NodeTest.FUNC_StoreAndFindBigValue)
ADD_MEMCHECK_IGNORE(NodeTest.FUNC_StoreAndFindMultipleValues)
ADD_MEMCHECK_IGNORE(NodeTest.FUNC_MultipleNodesFindSingleValue)
ADD_MEMCHECK_IGNORE(NodeTest.FUNC_ClientFindValue)
ADD_MEMCHECK_IGNORE(NodeTest.FUNC_GetContact)
ADD_MEMCHECK_IGNORE(NodeTest.FUNC_FindNonExistingValue)
ADD_MEMCHECK_IGNORE(NodeTest.FUNC_FindDeadNode)
ADD_MEMCHECK_IGNORE(NodeTest.FUNC_JoinLeave)
ADD_MEMCHECK_IGNORE(NodeTest.FUNC_StoreWithInvalidRequest)
ADD_MEMCHECK_IGNORE(NodeTest.FUNC_Update)
ADD_MEMCHECK_IGNORE(NodeTest.FUNC_FindNodes)
ADD_MEMCHECK_IGNORE(NodeTest.FUNC_Delete)
ADD_MEMCHECK_IGNORE(NodeTest.FUNC_InvalidDeleteRequest)
ADD_MEMCHECK_IGNORE(FullOrClient/NodeImplTest.FUNC_JoinLeave/0)
ADD_MEMCHECK_IGNORE(FullOrClient/NodeImplTest.FUNC_JoinLeave/1)
ADD_MEMCHECK_IGNORE(FullOrClient/NodeImplTest.FUNC_FindNodes/0)
ADD_MEMCHECK_IGNORE(FullOrClient/NodeImplTest.FUNC_FindNodes/1)
ADD_MEMCHECK_IGNORE(FullOrClient/NodeImplTest.FUNC_Store/0)
ADD_MEMCHECK_IGNORE(FullOrClient/NodeImplTest.FUNC_Store/1)
ADD_MEMCHECK_IGNORE(FullOrClient/NodeImplTest.FUNC_FindValue/0)
ADD_MEMCHECK_IGNORE(FullOrClient/NodeImplTest.FUNC_FindValue/1)
ADD_MEMCHECK_IGNORE(FullOrClient/NodeImplTest.FUNC_Delete/0)
ADD_MEMCHECK_IGNORE(FullOrClient/NodeImplTest.FUNC_Delete/1)
ADD_MEMCHECK_IGNORE(FullOrClient/NodeImplTest.FUNC_Update/1)
ADD_MEMCHECK_IGNORE(FullOrClient/NodeImplTest.FUNC_StoreRefresh/0)
ADD_MEMCHECK_IGNORE(FullOrClient/NodeImplTest.FUNC_StoreRefresh/1)
ADD_MEMCHECK_IGNORE(FullOrClient/NodeImplTest.FUNC_DeleteRefresh/0)
ADD_MEMCHECK_IGNORE(FullOrClient/NodeImplTest.FUNC_DeleteRefresh/1)
ADD_MEMCHECK_IGNORE(FullOrClient/NodeImplTest.FUNC_GetContact/0)
ADD_MEMCHECK_IGNORE(FullOrClient/NodeImplTest.FUNC_GetContact/1)
ADD_MEMCHECK_IGNORE(NodeChurnTest.FUNC_RandomStartStopNodes)


###################################################################################################
# Install details                                                                                 #
###################################################################################################
FINAL_MESSAGE()
RENAME_OUTDATED_BUILT_EXES()

SET(MAIDSAFE_DHT_INSTALL_FILES
      ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/config.h
      ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/contact.h
      ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/maidsafe-dht.h
      ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/message_handler.h
      ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/node-api.h
      ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/node_container.h
      ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/node_id.h
      ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/return_codes.h
      ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/rpcs_objects.h
      ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/version.h)

SET(MAIDSAFE_DHT_TEST_INSTALL_FILES
      ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/tests/local_network.h)

MS_INSTALL_LIBS(maidsafe_dht_static)
MS_INSTALL_HEADERS(dht ${MAIDSAFE_DHT_INSTALL_FILES})
MS_INSTALL_HEADERS(dht/tests ${MAIDSAFE_DHT_TEST_INSTALL_FILES})
MS_INSTALL_EXPORT()
