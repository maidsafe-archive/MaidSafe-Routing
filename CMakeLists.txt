#==============================================================================#
#                                                                              #
#  Copyright (c) 2010 maidsafe.net limited                                     #
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


CMAKE_MINIMUM_REQUIRED(VERSION 2.8.4 FATAL_ERROR)
MESSAGE("================================================================================\n")

SET(ERROR_MESSAGE_CMAKE_PATH "cmake ../../..")
GET_FILENAME_COMPONENT(CMAKE_BUILD_TYPE_DIR ${CMAKE_BINARY_DIR} NAME)

# Variable MSVC is not set until after PROJECT command below
IF(CMAKE_BUILD_TYPE_DIR MATCHES "Win_MSVC")
  SET(CMAKE_BUILD_TYPE_DIR Release)
  SET(ERROR_MESSAGE_CMAKE_PATH "   cmake ..\\..")
ELSEIF(CMAKE_BUILD_TYPE_DIR MATCHES "kdev")
  SET(CMAKE_BUILD_TYPE_DIR Debug)
  SET(KDEV 1)
ENDIF()

SET(ERROR_MESSAGE "No generator previously specified.\nTo see a full list of generators, run:\n\t${ERROR_MESSAGE_CMAKE_PATH} --help\n")
SET(CMAKE_GEN ${CMAKE_GENERATOR} CACHE INTERNAL "" FORCE)
IF(NOT ${CMAKE_C_COMPILER_WORKS})
  MESSAGE(FATAL_ERROR ${ERROR_MESSAGE})
ENDIF()

IF((CMAKE_BUILD_TYPE_DIR MATCHES "Release") OR (DEFINED ${KDEV}))
  SET(PROJECT_NAME maidsafe_dht)
ELSE()
  SET(PROJECT_NAME maidsafe_dht_${CMAKE_BUILD_TYPE_DIR})
ENDIF()
OPTION (BUILD_SHARED_LIBS "Build shared libraries." OFF)


# This call resets CMAKE_BUILD_TYPE, hence it's copied from CMAKE_BUILD_TYPE_DIR which is set above.
PROJECT(${PROJECT_NAME})
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
FILE(GLOB PROTO_FILES_KADEMLIA RELATIVE ${PROTO_SOURCE_DIR} "${PROTO_SOURCE_DIR}/maidsafe/dht/kademlia/*.proto")
FILE(GLOB PROTO_FILES_CRASH RELATIVE ${PROTO_SOURCE_DIR} "${PROTO_SOURCE_DIR}/maidsafe/dht/crash_reporter/*.proto")
SET(PROTO_FILES "${PROTO_FILES_KADEMLIA};${PROTO_FILES_CRASH}")
INCLUDE(maidsafe_standard_setup)
HANDLE_VERSIONS(${PROJECT_SOURCE_DIR}/src/maidsafe/dht/version.h)

###################################################################################################
# Set up all files as GLOBs                                                                       #
###################################################################################################
####    CODE    ####
FILE(GLOB SOURCE_FILES_MAIDSAFE_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/*.cc")
FILE(GLOB HEADER_FILES_MAIDSAFE_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/*.h")
SET(ALL_FILES_MAIDSAFE_DIR ${SOURCE_FILES_MAIDSAFE_DIR} ${HEADER_FILES_MAIDSAFE_DIR})
SOURCE_GROUP("maidsafe-dht Source Files" FILES ${SOURCE_FILES_MAIDSAFE_DIR})
SOURCE_GROUP("maidsafe-dht Header Files" FILES ${HEADER_FILES_MAIDSAFE_DIR})

FILE(GLOB SOURCE_FILES_KADEMLIA_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/*.cc")
FILE(GLOB HEADER_FILES_KADEMLIA_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/*.h")
SET(ALL_FILES_KADEMLIA_DIR ${SOURCE_FILES_KADEMLIA_DIR} ${HEADER_FILES_KADEMLIA_DIR})
SET(STYLE_KADEMLIA ${ALL_FILES_KADEMLIA_DIR})
LIST(REMOVE_ITEM STYLE_KADEMLIA "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/kademlia.pb.cc")
LIST(REMOVE_ITEM STYLE_KADEMLIA "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/kademlia.pb.h")
LIST(REMOVE_ITEM STYLE_KADEMLIA "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/rpcs.pb.cc")
LIST(REMOVE_ITEM STYLE_KADEMLIA "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/rpcs.pb.h")
SOURCE_GROUP("kademlia Source Files" FILES ${SOURCE_FILES_KADEMLIA_DIR})
SOURCE_GROUP("kademlia Header Files" FILES ${HEADER_FILES_KADEMLIA_DIR})

FILE(GLOB SOURCE_FILES_CRASH_REPORTER_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/crash_reporter/*.cc")
FILE(GLOB HEADER_FILES_CRASH_REPORTER_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/crash_reporter/*.h")
SET(ALL_FILES_CRASH_REPORTER_DIR ${SOURCE_FILES_CRASH_REPORTER_DIR} ${HEADER_FILES_CRASH_REPORTER_DIR})
SOURCE_GROUP("crash_reporter Source Files" FILES ${SOURCE_FILES_CRASH_REPORTER_DIR})
SOURCE_GROUP("crash_reporter Header Files" FILES ${HEADER_FILES_CRASH_REPORTER_DIR})

SET(ALL_FILES_CRASH_SERVER_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/crash_server/main.cc")
SOURCE_GROUP("crash_server Source Files" FILES ${ALL_FILES_CRASH_SERVER_DIR})

####    BENCHMARK, DEMO & TESTS    ####
FILE(GLOB SOURCE_FILES_BENCHMARK_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/benchmark/*.cc")
FILE(GLOB HEADER_FILES_BENCHMARK_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/benchmark/*.h")
SET(ALL_FILES_BENCHMARK_DIR ${SOURCE_FILES_BENCHMARK_DIR} ${HEADER_FILES_BENCHMARK_DIR})
SOURCE_GROUP("Benchmark Source Files" FILES ${SOURCE_FILES_BENCHMARK_DIR})
SOURCE_GROUP("Benchmark Header Files" FILES ${HEADER_FILES_BENCHMARK_DIR})

FILE(GLOB SOURCE_FILES_DEMO_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/demo/*.cc")
FILE(GLOB HEADER_FILES_DEMO_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/demo/*.h")
SET(ALL_FILES_DEMO_DIR ${SOURCE_FILES_DEMO_DIR} ${HEADER_FILES_DEMO_DIR})
SOURCE_GROUP("Demo Source Files" FILES ${SOURCE_FILES_DEMO_DIR})
SOURCE_GROUP("Demo Header Files" FILES ${HEADER_FILES_DEMO_DIR})

FILE(GLOB SOURCE_FILES_TEST_KADEMLIA_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/tests/*.cc")
FILE(GLOB HEADER_FILES_TEST_KADEMLIA_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/tests/*.h")
SET(ALL_FILES_TEST_KADEMLIA_DIR ${SOURCE_FILES_TEST_KADEMLIA_DIR} ${HEADER_FILES_TEST_KADEMLIA_DIR})
SOURCE_GROUP("Kademlia Test Source Files" FILES ${SOURCE_FILES_TEST_KADEMLIA_DIR})
SOURCE_GROUP("Kademlia Test Header Files" FILES ${HEADER_FILES_TEST_KADEMLIA_DIR})

FILE(GLOB SOURCE_FILES_TEST_FUNCTIONAL_KADEMLIA_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/tests/functional/*.cc")
FILE(GLOB HEADER_FILES_TEST_FUNCTIONAL_KADEMLIA_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/tests/functional/*.h")
SET(ALL_FILES_TEST_FUNCTIONAL_KADEMLIA_DIR ${SOURCE_FILES_TEST_FUNCTIONAL_KADEMLIA_DIR} ${HEADER_FILES_TEST_FUNCTIONAL_KADEMLIA_DIR})
SOURCE_GROUP("Functional Test Source Files" FILES ${SOURCE_FILES_TEST_FUNCTIONAL_KADEMLIA_DIR})
SOURCE_GROUP("Functional Test Header Files" FILES ${HEADER_FILES_TEST_FUNCTIONAL_KADEMLIA_DIR})

SET(SOURCE_FILES_TEST_FUNCTIONAL_NODE "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/tests/functional/node_test.cc"
                                      "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/tests/test_utils.cc"
                                      "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/tests/test_main.cc")
SET(SOURCE_FILES_TEST_FUNCTIONAL_NODE_IMPL "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/tests/functional/node_impl_test.cc"
                                           "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/tests/test_utils.cc"
                                           "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/tests/test_main.cc")
SET(SOURCE_FILES_TEST_FUNCTIONAL_NODE_CHURN "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/tests/functional/node_churn_test.cc"
                                            "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/tests/test_utils.cc"
                                            "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/tests/test_main.cc")
SET(HEADER_FILES_TEST_FUNCTIONALS "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/tests/functional/test_node_environment.h"
                                  "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/tests/test_utils.h"
                                  "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/tests/local_network.h")
# IF(Mysqlpp_FOUND)
#   FILE(GLOB ALL_FILES_NETWORK_TEST_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/tests/distributed_network/*.cc"
#                                        "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/tests/distributed_network/*.h")
# ENDIF()


###################################################################################################
# All Platforms:  Add Test Exes and Create all maidsafe_dht libs                                  #
###################################################################################################
ADD_EXECUTABLE(TESTkademlia ${ALL_FILES_TEST_KADEMLIA_DIR})
ADD_EXECUTABLE(TESTnode_functional ${SOURCE_FILES_TEST_FUNCTIONAL_NODE} ${HEADER_FILES_TEST_FUNCTIONALS})
ADD_EXECUTABLE(TESTnode_impl_functional ${SOURCE_FILES_TEST_FUNCTIONAL_NODE_IMPL} ${HEADER_FILES_TEST_FUNCTIONALS})
ADD_EXECUTABLE(TESTnode_churn ${SOURCE_FILES_TEST_FUNCTIONAL_NODE_CHURN} ${HEADER_FILES_TEST_FUNCTIONALS})
ADD_EXECUTABLE(KademliaDemo ${ALL_FILES_DEMO_DIR})
ADD_EXECUTABLE(benchmark_static ${ALL_FILES_BENCHMARK_DIR})
IF(Mysqlpp_FOUND)
  ADD_EXECUTABLE(distributed_net_clients EXCLUDE_FROM_ALL ${ALL_FILES_NETWORK_TEST_DIR})
ENDIF()
ADD_EXECUTABLE(CrashReporter ${ALL_FILES_CRASH_REPORTER_DIR})
IF(UNIX)
	ADD_EXECUTABLE(CrashServer ${ALL_FILES_CRASH_SERVER_DIR})
ENDIF()
# Force renaming of exes to match standard CMake library renaming policy
RENAME_EXECUTABLE(TESTkademlia)
RENAME_EXECUTABLE(TESTnode_functional)
RENAME_EXECUTABLE(TESTnode_impl_functional)
RENAME_EXECUTABLE(TESTnode_churn)
RENAME_EXECUTABLE(KademliaDemo)
RENAME_EXECUTABLE(benchmark_static)
IF(Mysqlpp_FOUND)
  RENAME_EXECUTABLE(distributed_net_clients)
ENDIF()
RENAME_EXECUTABLE(CrashReporter)
IF(UNIX)
  RENAME_EXECUTABLE(CrashServer)
ENDIF()

SET(TEST_TARGETS TESTkademlia TESTnode_functional TESTnode_impl_functional TESTnode_churn)
SET_TARGET_PROPERTIES(${TEST_TARGETS} PROPERTIES FOLDER "Tests")
SET_TARGET_PROPERTIES(KademliaDemo PROPERTIES FOLDER "Demo")
SET_TARGET_PROPERTIES(benchmark_static PROPERTIES FOLDER "Benchmark")
IF(Mysqlpp_FOUND)
  SET_TARGET_PROPERTIES(distributed_net_clients PROPERTIES FOLDER "Tests")
ENDIF()
SET_TARGET_PROPERTIES(CrashReporter PROPERTIES FOLDER "Crash Reporter")
IF(UNIX)
  SET_TARGET_PROPERTIES(CrashServer PROPERTIES FOLDER "Crash Server")
ENDIF()
SET_TARGET_PROPERTIES(TESTnode_functional PROPERTIES COMPILE_DEFINITIONS "FUNCTIONAL_NODE_TEST")
SET_TARGET_PROPERTIES(TESTnode_impl_functional PROPERTIES COMPILE_DEFINITIONS "FUNCTIONAL_NODE_IMPL_TEST")
SET_TARGET_PROPERTIES(TESTnode_churn PROPERTIES COMPILE_DEFINITIONS "FUNCTIONAL_CHURN_TEST")

ADD_LIBRARY(maidsafe_dht_static STATIC ${ALL_FILES_MAIDSAFE_DIR} ${ALL_FILES_KADEMLIA_DIR})

#Rename maidsafe_dht_static as maidsafe_dht on way out NOT in MSVC names clash of static and import lib
SET_TARGET_PROPERTIES(maidsafe_dht_static PROPERTIES OUTPUT_NAME maidsafe_dht)

SET_TARGET_PROPERTIES(maidsafe_dht_static PROPERTIES FOLDER "MaidSafe Libraries")

TARGET_LINK_LIBRARIES(maidsafe_dht_static
                        maidsafe_transport_static
                        maidsafe_common_static
                        cryptopp_static
                        protobuf_static
                        glog_static
                        breakpad_static
                        boost_date_time
                        boost_filesystem
                        boost_regex
                        boost_system
                        boost_thread
                        ${SYS_LIB})
TARGET_LINK_LIBRARIES(TESTkademlia maidsafe_dht_static gmock_static)
TARGET_LINK_LIBRARIES(TESTnode_functional maidsafe_dht_static gtest_static)
TARGET_LINK_LIBRARIES(TESTnode_impl_functional maidsafe_dht_static gtest_static)
TARGET_LINK_LIBRARIES(TESTnode_churn maidsafe_dht_static gtest_static)
TARGET_LINK_LIBRARIES(KademliaDemo maidsafe_dht_static boost_program_options boost_serialization breakpad_static)
TARGET_LINK_LIBRARIES(benchmark_static maidsafe_dht_static boost_program_options)
IF(Mysqlpp_FOUND)
  TARGET_LINK_LIBRARIES(distributed_net_clients maidsafe_dht_static ${Mysqlpp_LIBRARY})
ENDIF()
TARGET_LINK_LIBRARIES(CrashReporter
                        maidsafe_transport_static
                        maidsafe_common_static
                        protobuf_static
                        glog_static
                        breakpad_static
                        boost_filesystem
                        boost_system
                        boost_thread)
IF(UNIX)
  TARGET_LINK_LIBRARIES(CrashServer
                          maidsafe_transport_static
                          maidsafe_common_static
                          protobuf_static
                          glog_static
                          breakpad_static
                          boost_filesystem
                          boost_system
                          boost_thread)
ENDIF()


###################################################################################################
# Builds                                                                                          #
###################################################################################################
INCLUDE(maidsafe_standard_flags)

IF(MSVC)
  # C4503 'decorated name length exceeded' caused by boost multi-index and signals2
  # Disabled as per advice at https://svn.boost.org/trac/boost/wiki/Guidelines/WarningsGuidelines
  # C4512 'assignment operator could not be generated' caused by boost signals2
  # Disabled as per advice at http://lists.boost.org/boost-users/2009/01/44368.php
  # C4996 'Function call with parameters that may be unsafe' caused by boost signals2
  # Disabled as per advice at https://svn.boost.org/trac/boost/wiki/Guidelines/WarningsGuidelines
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4503 /wd4512 /wd4996")

  # Warnings switched off for protocol buffer generated files and libupnp files
  SET_SOURCE_FILES_PROPERTIES(${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/kademlia.pb.cc
                              ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/rpcs.pb.cc
                              ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/crash_reporter/crash.pb.cc
                                PROPERTIES COMPILE_FLAGS "/W0")

  SET_TARGET_PROPERTIES(maidsafe_dht_static TESTkademlia PROPERTIES COMPILE_FLAGS "/bigobj")

  SET_TARGET_PROPERTIES(TESTkademlia TESTnode_functional TESTnode_impl_functional TESTnode_churn KademliaDemo benchmark_static CrashReporter PROPERTIES
                          LINK_FLAGS_RELEASE "/OPT:REF /OPT:ICF /LTCG /INCREMENTAL:NO ${LINKER_LIBS_DIRS_RELEASE}"
                          LINK_FLAGS_DEBUG "${LINKER_LIBS_DIRS_DEBUG}"
                          LINK_FLAGS_RELWITHDEBINFO "${LINKER_LIBS_DIRS_DEBUG}"
                          LINK_FLAGS_MINSIZEREL "${LINKER_LIBS_DIRS_DEBUG}")
  IF(Mysqlpp_FOUND)
    SET_TARGET_PROPERTIES(distributed_net_clients PROPERTIES
                            LINK_FLAGS_RELEASE "/OPT:REF /OPT:ICF /LTCG /INCREMENTAL:NO ${LINKER_LIBS_DIRS_RELEASE}"
                            LINK_FLAGS_DEBUG "${LINKER_LIBS_DIRS_DEBUG}"
                            LINK_FLAGS_RELWITHDEBINFO "${LINKER_LIBS_DIRS_DEBUG}"
                            LINK_FLAGS_MINSIZEREL "${LINKER_LIBS_DIRS_DEBUG}")
  ENDIF()

  SET_TARGET_PROPERTIES(maidsafe_dht_static PROPERTIES STATIC_LIBRARY_FLAGS_RELEASE "/LTCG")
ELSEIF(UNIX)
  SET_TARGET_PROPERTIES(KademliaDemo PROPERTIES COMPILE_FLAGS "-g")
  FILE(GLOB PROTO_SOURCE_FILES "${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/*.pb.cc")
  SET_SOURCE_FILES_PROPERTIES(${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/service.cc
                              ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/node.cc
                              ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/node_impl.cc
                              ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/benchmark/benchmark.cc
                              ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/tests/test_main.cc
                              ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/tests/mock_rpcs_test.cc
                              ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/tests/mock_node_impl_test.cc
                              ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/tests/rpcs_test.cc
                              ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/tests/services_test.cc
                              ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/tests/functional/node_churn_test
                              ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/tests/functional/node_test.cc
                              ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/tests/functional/node_impl_test.cc
                              PROPERTIES COMPILE_FLAGS "-Wno-effc++")
  SET_SOURCE_FILES_PROPERTIES(${PROTO_SOURCE_FILES} PROPERTIES COMPILE_FLAGS "-w")
ENDIF()


###################################################################################################
# Tests                                                                                           #
###################################################################################################
IF(PYTHONINTERP_FOUND)
  IF(UNIX)
    ADD_TEST(STYLE_CHECK python ${MaidSafeCommon_TOOLS_DIR}/cpplint.py
               ${STYLE_KADEMLIA}
               ${ALL_FILES_BENCHMARK_DIR}
               ${ALL_FILES_DEMO_DIR}
               ${ALL_FILES_TEST_KADEMLIA_DIR}
               ${ALL_FILES_TEST_FUNCTIONAL_KADEMLIA_DIR}
               ${ALL_FILES_CRASH_REPORTER_DIR}
               ${ALL_FILES_CRASH_SERVER_DIR})
    SET_PROPERTY(TEST STYLE_CHECK PROPERTY LABELS Functional CodingStyle)
  ELSE()
    STRING(REPLACE "/" "\\\\" STYLE_CHECK_SOURCE ${PROJECT_SOURCE_DIR})
    ADD_TEST(STYLE_CHECK ${MaidSafeCommon_TOOLS_DIR}/run_cpplint.bat ${STYLE_CHECK_SOURCE} ${MaidSafeCommon_TOOLS_DIR}/cpplint.py)
    SET_PROPERTY(TEST STYLE_CHECK PROPERTY LABELS Functional CodingStyle)
  ENDIF()
ENDIF()

FOREACH(TEST_TARGET ${TEST_TARGETS})
  ADD_GTESTS(${TEST_TARGET})
ENDFOREACH()

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

SET(MAIDSAFE_KAD_INSTALL_FILES
      ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/node-api.h
      ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/node_container.h
      ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/config.h
      ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/contact.h
      ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/node_id.h
      ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/message_handler.h
      ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/rpcs_objects.h
      ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/return_codes.h)
FOREACH(MAIDSAFE_KAD_INSTALL_FILE ${MAIDSAFE_KAD_INSTALL_FILES})
  CHECK_INSTALL_HEADER_HAS_VERSION_GUARD(${MAIDSAFE_KAD_INSTALL_FILE})
ENDFOREACH()

SET(MAIDSAFE_KAD_TEST_INSTALL_FILES
      ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/kademlia/tests/local_network.h)
FOREACH(MAIDSAFE_KAD_TEST_INSTALL_FILE ${MAIDSAFE_KAD_TEST_INSTALL_FILES})
  CHECK_INSTALL_HEADER_HAS_VERSION_GUARD(${MAIDSAFE_KAD_TEST_INSTALL_FILE})
ENDFOREACH()

SET(MAIDSAFE_MAIN_INSTALL_FILES
      ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/maidsafe-dht.h
      ${PROJECT_SOURCE_DIR}/src/maidsafe/dht/version.h)
FOREACH(MAIDSAFE_MAIN_INSTALL_FILE ${MAIDSAFE_MAIN_INSTALL_FILES})
  CHECK_INSTALL_HEADER_HAS_VERSION_GUARD(${MAIDSAFE_MAIN_INSTALL_FILE})
ENDFOREACH()

SET(EXPORT_NAME maidsafe_dht)
INSTALL(TARGETS maidsafe_dht_static EXPORT ${EXPORT_NAME} ARCHIVE DESTINATION lib)
INSTALL(FILES ${MAIDSAFE_KAD_INSTALL_FILES} DESTINATION include/maidsafe/dht/kademlia)
INSTALL(FILES ${MAIDSAFE_KAD_TEST_INSTALL_FILES} DESTINATION include/maidsafe/dht/kademlia/tests)
INSTALL(FILES ${MAIDSAFE_MAIN_INSTALL_FILES} DESTINATION include/maidsafe/dht)
INSTALL(EXPORT ${EXPORT_NAME} DESTINATION share/maidsafe)
ADD_VERSION_INFO_TO_INSTALLED_FILE()
