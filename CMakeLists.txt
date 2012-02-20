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


SET(MS_PROJECT_NAME maidsafe_routing)

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
FILE(GLOB PROTO_FILES_KADEMLIA RELATIVE ${PROTO_SOURCE_DIR} "${PROTO_SOURCE_DIR}/maidsafe/routing/*.proto")
FILE(GLOB PROTO_FILES_TESTS RELATIVE ${PROTO_SOURCE_DIR} "${PROTO_SOURCE_DIR}/maidsafe/routing/tests/*.proto")
SET(PROTO_FILES "${PROTO_FILES_KADEMLIA};${PROTO_FILES_TESTS}")
INCLUDE(maidsafe_standard_setup)
HANDLE_VERSIONS(${PROJECT_SOURCE_DIR}/src/maidsafe/routing/version.h)


###################################################################################################
# Set up all files as GLOBs                                                                       #
###################################################################################################
####    CODE    ####
FILE(GLOB SOURCE_FILES_ROUTING_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/*.cc")
FILE(GLOB HEADER_FILES_ROUTING_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/*.h")
SET(ALL_FILES_ROUTING_DIR ${SOURCE_FILES_ROUTING_DIR} ${HEADER_FILES_ROUTING_DIR})
SET(STYLE_ROUTING ${ALL_FILES_ROUTING_DIR})
LIST(REMOVE_ITEM STYLE_ROUTING "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/routing.pb.cc")
LIST(REMOVE_ITEM STYLE_ROUTING "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/routing.pb.h")
SOURCE_GROUP("ROUTING Source Files" FILES ${SOURCE_FILES_ROUTING_DIR})
SOURCE_GROUP("ROUTING Header Files" FILES ${HEADER_FILES_ROUTING_DIR})

####    BENCHMARK, DEMO & TESTS    ####
FILE(GLOB SOURCE_FILES_BENCHMARK_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/benchmark/*.cc")
FILE(GLOB HEADER_FILES_BENCHMARK_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/benchmark/*.h")
SET(ALL_FILES_BENCHMARK_DIR ${SOURCE_FILES_BENCHMARK_DIR} ${HEADER_FILES_BENCHMARK_DIR})
SOURCE_GROUP("Benchmark Source Files" FILES ${SOURCE_FILES_BENCHMARK_DIR})
SOURCE_GROUP("Benchmark Header Files" FILES ${HEADER_FILES_BENCHMARK_DIR})

FILE(GLOB SOURCE_FILES_DEMO_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/demo/*.cc")
FILE(GLOB HEADER_FILES_DEMO_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/demo/*.h")
SET(ALL_FILES_DEMO_DIR ${SOURCE_FILES_DEMO_DIR} ${HEADER_FILES_DEMO_DIR})
SOURCE_GROUP("Demo Source Files" FILES ${SOURCE_FILES_DEMO_DIR})
SOURCE_GROUP("Demo Header Files" FILES ${HEADER_FILES_DEMO_DIR})

FILE(GLOB SOURCE_FILES_ROUTING_TESTS_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/*.cc")
FILE(GLOB HEADER_FILES_ROUTING_TESTS_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/*.h")
SET(ALL_FILES_ROUTING_TESTS_DIR ${SOURCE_FILES_ROUTING_TESTS_DIR} ${HEADER_FILES_ROUTING_TESTS_DIR})
SET(STYLE_ROUTING_TESTS ${ALL_FILES_ROUTING_TESTS_DIR})
SOURCE_GROUP("ROUTING Tests Source Files" FILES ${SOURCE_FILES_ROUTING_TESTS_DIR})
SOURCE_GROUP("ROUTING Tests Header Files" FILES ${HEADER_FILES_ROUTING_TESTS_DIR})
LIST(REMOVE_ITEM STYLE_ROUTING_TESTS "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/wrapper.pb.cc")
LIST(REMOVE_ITEM STYLE_ROUTING_TESTS "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/wrapper.pb.h")

FILE(GLOB SOURCE_FILES_ROUTING_TESTS_FUNCTIONAL_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/functional/*.cc")
FILE(GLOB HEADER_FILES_ROUTING_TESTS_FUNCTIONAL_DIR "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/functional/*.h")
SET(ALL_FILES_ROUTING_TESTS_FUNCTIONAL_DIR ${SOURCE_FILES_ROUTING_TESTS_FUNCTIONAL_DIR} ${HEADER_FILES_ROUTING_TESTS_FUNCTIONAL_DIR})
SOURCE_GROUP("Functional Tests Source Files" FILES ${SOURCE_FILES_ROUTING_TESTS_FUNCTIONAL_DIR})
SOURCE_GROUP("Functional Tests Header Files" FILES ${HEADER_FILES_ROUTING_TESTS_FUNCTIONAL_DIR})

SET(SOURCE_FILES_ROUTING_TESTS_FUNCTIONAL_NODE "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/functional/node_test.cc"
                                           "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/test_utils.cc"
                                           "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/test_main.cc")
SET(SOURCE_FILES_ROUTING_TESTS_FUNCTIONAL_NODE_IMPL "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/functional/node_impl_test.cc"
                                                "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/test_utils.cc"
                                                "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/test_main.cc")
SET(SOURCE_FILES_ROUTING_TESTS_FUNCTIONAL_NODE_CHURN "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/functional/node_churn_test.cc"
                                                 "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/test_utils.cc"
                                                 "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/test_main.cc")
SET(HEADER_FILES_ROUTING_TESTS_FUNCTIONALS "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/functional/test_node_environment.h"
                                       "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/test_utils.h"
                                       "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/local_network.h")


###################################################################################################
# Define MaidSafe libraries and executables                                                       #
###################################################################################################
MS_ADD_STATIC_LIBRARY(${MS_PROJECT_NAME} ${ALL_FILES_ROUTING_DIR})
MS_ADD_EXECUTABLE(TESTRouting Tests ${ALL_FILES_ROUTING_TESTS_DIR})

TARGET_LINK_LIBRARIES(maidsafe_routing_static
                        maidsafe_transport_static
                        maidsafe_common_static
                        breakpad_static
                        boost_date_time
                        boost_regex)
TARGET_LINK_LIBRARIES(TESTRouting maidsafe_routing_static gmock_static)



###################################################################################################
# Set compiler and linker flags                                                                   #
###################################################################################################
INCLUDE(maidsafe_standard_flags)

IF(MSVC)
  # Warnings switched off for protocol buffer generated files
  SET_SOURCE_FILES_PROPERTIES(${PROJECT_SOURCE_DIR}/src/maidsafe/routing/routing.pb.cc
                                PROPERTIES COMPILE_FLAGS "/W0")
  SET_TARGET_PROPERTIES(maidsafe_routing_static TESTRouting PROPERTIES COMPILE_FLAGS "/bigobj")
  SET_TARGET_PROPERTIES(maidsafe_routing_static PROPERTIES STATIC_LIBRARY_FLAGS_RELEASE "/LTCG")
ELSEIF(UNIX)
#  SET_TARGET_PROPERTIES(KademliaDemo PROPERTIES COMPILE_FLAGS "-g")
  FILE(GLOB PROTO_SOURCE_FILES "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/*.pb.cc"
                               "${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/*.pb.cc")
#   SET_SOURCE_FILES_PROPERTIES(${PROJECT_SOURCE_DIR}/src/maidsafe/routing/service.cc
#                               ${PROJECT_SOURCE_DIR}/src/maidsafe/routing/node.cc
#                               ${PROJECT_SOURCE_DIR}/src/maidsafe/routing/node_impl.cc
#                               ${PROJECT_SOURCE_DIR}/src/maidsafe/routing/benchmark/benchmark.cc
#                               ${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/test_main.cc
#                               ${PROJECT_SOURCE_DIR}/src/maidsafe/routing/tests/mock_rpcs_test.cc
#                               PROPERTIES COMPILE_FLAGS "-Wno-effc++")
  SET_SOURCE_FILES_PROPERTIES(${PROTO_SOURCE_FILES} PROPERTIES COMPILE_FLAGS "-w")
ENDIF()


###################################################################################################
# Tests                                                                                           #
###################################################################################################
ADD_STYLE_TEST(${STYLE_ROUTING}
               ${ALL_FILES_BENCHMARK_DIR}
               ${ALL_FILES_DEMO_DIR}
               ${STYLE_ROUTING_TESTS}
               ${ALL_FILES_ROUTING_TESTS_FUNCTIONAL_DIR})

ADD_GTESTS(TESTRouting)


TEST_SUMMARY_OUTPUT()

# Remove most lengthy tests and style checkers from MemCheck runs.
ADD_MEMCHECK_IGNORE(STYLE_CHECK)



###################################################################################################
# Install details                                                                                 #
###################################################################################################
FINAL_MESSAGE()
RENAME_OUTDATED_BUILT_EXES()

SET(MAIDSAFE_ROUTING_INSTALL_FILES
      ${PROJECT_SOURCE_DIR}/src/maidsafe/routing/maidsafe_routing.h
      ${PROJECT_SOURCE_DIR}/src/maidsafe/routing/version.h)


MS_INSTALL_LIBS(maidsafe_routing_static)
MS_INSTALL_HEADERS(routing ${MAIDSAFE_ROUTING_INSTALL_FILES})
MS_INSTALL_HEADERS(routing/tests ${MAIDSAFE_ROUTING_TEST_INSTALL_FILES})
MS_INSTALL_EXPORT()
