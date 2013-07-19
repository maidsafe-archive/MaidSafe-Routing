#==================================================================================================#
#                                                                                                  #
#  Copyright 2012 MaidSafe.net limited                                                             #
#                                                                                                  #
#  This MaidSafe Software is licensed under the MaidSafe.net Commercial License, version 1.0 or    #
#  later, and The General Public License (GPL), version 3. By contributing code to this project    #
#  You agree to the terms laid out in the MaidSafe Contributor Agreement, version 1.0, found in    #
#  the root directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also    #
#  available at:                                                                                   #
#                                                                                                  #
#    http://www.novinet.com/license                                                                #
#                                                                                                  #
#  Unless required by applicable law or agreed to in writing, software distributed under the       #
#  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,       #
#  either express or implied. See the License for the specific language governing permissions      #
#  and limitations under the License.                                                              #
#                                                                                                  #
#==================================================================================================#


set(Qt5Required OFF)
include(../../../../../../../cmake_modules/maidsafe_find_qt5.cmake)
if(NOT Qt5Core_FOUND)
  message(STATUS "Didn't find Qt5. Not building Network Viewer tool")
  return()
endif()

set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

cmake_minimum_required(VERSION 2.8.10 FATAL_ERROR)
project(network_viewer)

include(../../../../../../../cmake_modules/standard_setup.cmake)
# Enable auto-linking qtmain for Windows
cmake_policy(SET CMP0020 NEW)

include_directories(${common_SOURCE_DIR}/include)
include_directories(${passport_SOURCE_DIR}/include)
include_directories(${private_SOURCE_DIR}/include)
include_directories(${rudp_SOURCE_DIR}/include)
include_directories(${routing_SOURCE_DIR}/include)


#==================================================================================================#
# Set up all Common Network-Viewer files as GLOBs                                                  #
#==================================================================================================#
set(NETWORK_VIEWER_MAIN ${PROJECT_SOURCE_DIR}/main.cc)
source_group("Auto Compiled\\QObject Files" FILES "${CMAKE_CURRENT_BINARY_DIR}/network_viewer_automoc.cpp")

file(GLOB SOURCE_FILES_NETWORK_VIEWER_HELPERS_DIR ${PROJECT_SOURCE_DIR}/helpers/*.cc)
file(GLOB HEADER_FILES_NETWORK_VIEWER_HELPERS_DIR ${PROJECT_SOURCE_DIR}/helpers/*.h)
source_group("Helpers\\Source Files" FILES ${SOURCE_FILES_NETWORK_VIEWER_HELPERS_DIR})
source_group("Helpers\\Header Files" FILES ${HEADER_FILES_NETWORK_VIEWER_HELPERS_DIR})

set(NETWORK_VIEWER_PCH_HEADER ${PROJECT_SOURCE_DIR}/helpers/qt_push_headers.h)
if(NOT WIN32)
  list(REMOVE_ITEM SOURCE_FILES_NETWORK_VIEWER_HELPERS_DIR ${PROJECT_SOURCE_DIR}/helpers/qt_push_headers.cc)
endif()

file(GLOB SOURCE_FILES_NETWORK_VIEWER_MODELS_DIR "${PROJECT_SOURCE_DIR}/models/*.cc")
file(GLOB HEADER_FILES_NETWORK_VIEWER_MODELS_DIR "${PROJECT_SOURCE_DIR}/models/*.h")
source_group("Models\\Source Files" FILES ${SOURCE_FILES_NETWORK_VIEWER_MODELS_DIR})
source_group("Models\\Header Files" FILES ${HEADER_FILES_NETWORK_VIEWER_MODELS_DIR})

file(GLOB SOURCE_FILES_NETWORK_VIEWER_CONTROLLERS_DIR "${PROJECT_SOURCE_DIR}/controllers/*.cc")
file(GLOB HEADER_FILES_NETWORK_VIEWER_CONTROLLERS_DIR "${PROJECT_SOURCE_DIR}/controllers/*.h")
source_group("Controllers\\Source Files" FILES ${SOURCE_FILES_NETWORK_VIEWER_CONTROLLERS_DIR})
source_group("Controllers\\Header Files" FILES ${HEADER_FILES_NETWORK_VIEWER_CONTROLLERS_DIR})

file(GLOB QRC_FILES_NETWORK_VIEWER_RESOURCES_DIR ${PROJECT_SOURCE_DIR}/resources/webkit_resources.qrc)
QT5_ADD_RESOURCES(RESOURCE_FILES_NETWORK_VIEWER_RESOURCES_DIR ${QRC_FILES_NETWORK_VIEWER_RESOURCES_DIR})
source_group("Auto Compiled\\Resource Files" FILES ${RESOURCE_FILES_NETWORK_VIEWER_RESOURCES_DIR})

file(GLOB UI_FILES_NETWORK_VIEWER_VIEWS_DIR ${PROJECT_SOURCE_DIR}/views/*.ui)
QT5_WRAP_UI(UIC_FILES_NETWORK_VIEWER_VIEWS_DIR ${UI_FILES_NETWORK_VIEWER_VIEWS_DIR})
source_group("Views" FILES ${UI_FILES_NETWORK_VIEWER_VIEWS_DIR})
source_group("Auto Compiled\\View Compiled Files" FILES ${UIC_FILES_NETWORK_VIEWER_VIEWS_DIR})

set(ALL_NETWORK_VIEWER_SOURCE_FILES
      ${SOURCE_FILES_NETWORK_VIEWER_HELPERS_DIR}
      ${SOURCE_FILES_NETWORK_VIEWER_MODELS_DIR}
      ${SOURCE_FILES_NETWORK_VIEWER_CONTROLLERS_DIR}
      ${UI_FILES_NETWORK_VIEWER_VIEWS_DIR}
      ${UIC_FILES_NETWORK_VIEWER_VIEWS_DIR})
set(ALL_NETWORK_VIEWER_HEADER_FILES
      ${HEADER_FILES_NETWORK_VIEWER_HELPERS_DIR}
      ${HEADER_FILES_NETWORK_VIEWER_MODELS_DIR}
      ${HEADER_FILES_NETWORK_VIEWER_CONTROLLERS_DIR})

####    Set Source Resource File Groups    ####
file(GLOB NETWORK_VIEWER_RESOURCE_FILES_ROOT_DIR ${PROJECT_SOURCE_DIR}/resources/*.*)

source_group("Resource Files" FILES ${NETWORK_VIEWER_RESOURCE_FILES_ROOT_DIR})

file(GLOB NETWORK_VIEWER_RESOURCE_FILES_CSS_DIR ${PROJECT_SOURCE_DIR}/resources/css/*.*)
source_group("Resource Files\\CSS" FILES ${NETWORK_VIEWER_RESOURCE_FILES_CSS_DIR})

file(GLOB NETWORK_VIEWER_RESOURCE_FILES_JS_DIR ${PROJECT_SOURCE_DIR}/resources/js/*.*)
source_group("Resource Files\\JS" FILES ${NETWORK_VIEWER_RESOURCE_FILES_JS_DIR})

file(GLOB NETWORK_VIEWER_RESOURCE_FILES_SCRIPTS_DIR ${PROJECT_SOURCE_DIR}/resources/scripts/*.*)
source_group("Resource Files\\Scripts" FILES ${NETWORK_VIEWER_RESOURCE_FILES_SCRIPTS_DIR})

file(GLOB NETWORK_VIEWER_RESOURCE_FILES_ICONS_DIR ${PROJECT_SOURCE_DIR}/resources/icons/*.*)
source_group("Resource Files\\Icons" FILES ${NETWORK_VIEWER_RESOURCE_FILES_ICONS_DIR})

set(ALL_NETWORK_VIEWER_RESOURCE_SOURCE_FILES
      ${NETWORK_VIEWER_RESOURCE_FILES_ROOT_DIR}
      ${NETWORK_VIEWER_RESOURCE_FILES_CSS_DIR}
      ${NETWORK_VIEWER_RESOURCE_FILES_JS_DIR}
      ${NETWORK_VIEWER_RESOURCE_FILES_ICONS_DIR}
      ${NETWORK_VIEWER_RESOURCE_FILES_SCRIPTS_DIR})


#==================================================================================================#
# Define MaidSafe libraries and executables                                                        #
#==================================================================================================#
ms_add_executable(network_viewer "Tools"
                      ${NETWORK_VIEWER_MAIN}
                      ${ALL_NETWORK_VIEWER_SOURCE_FILES}
                      ${ALL_NETWORK_VIEWER_HEADER_FILES}
                      ${RESOURCE_FILES_NETWORK_VIEWER_RESOURCES_DIR}
                      ${ALL_NETWORK_VIEWER_RESOURCE_SOURCE_FILES})
set(AllExesForCurrentProject ${AllExesForCurrentProject} PARENT_SCOPE)
add_dependencies(network_viewer boost)
if(WIN32)
  set_target_properties(network_viewer PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
qt5_use_modules(network_viewer Core Gui Widgets Network WebKit WebKitWidgets)

target_link_libraries(network_viewer maidsafe_common maidsafe_network_viewer)

rename_outdated_built_exes()


#==================================================================================================#
# Set compiler and linker flags                                                                    #
#==================================================================================================#
include(standard_flags)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DQT_NO_DEBUG_OUTPUT -DQT_NO_WARNING_OUTPUT -DQT_NO_DEBUG_STREAM")

if(WIN32)
  string(REGEX REPLACE "/Zm[0-9]+ " "/Zm500 " CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  set_source_files_properties(${ALL_NETWORK_VIEWER_SOURCE_FILES} PROPERTIES
                              COMPILE_FLAGS "/Yu\"${NETWORK_VIEWER_PCH_HEADER}\" /FI\"${NETWORK_VIEWER_PCH_HEADER}\"")
  set_source_files_properties("${PROJECT_SOURCE_DIR}/helpers/qt_push_headers.cc" PROPERTIES
                              COMPILE_FLAGS "/Yc\"${NETWORK_VIEWER_PCH_HEADER}\" /FI\"${NETWORK_VIEWER_PCH_HEADER}\"")
else(UNIX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-stack-protector -Wno-effc++")
endif()


#==================================================================================================#
# Tests                                                                                            #
#==================================================================================================#
if(MaidsafeTesting)
  add_style_test()
endif()

message(STATUS "Found Qt5. Network Viewer tool enabled.")
