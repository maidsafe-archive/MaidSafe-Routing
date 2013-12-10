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


set(Qt5Required OFF)
set(Qt5RequiredLibs
      Qt5Concurrent
      Qt5Core
      Qt5Gui
      Qt5Multimedia
      Qt5MultimediaWidgets
      Qt5Network
      Qt5OpenGL
      Qt5Positioning
      Qt5PrintSupport
      Qt5Qml
      Qt5Quick
      Qt5Sensors
      Qt5Sql
      Qt5WebKit
      Qt5WebKitWidgets
      Qt5Widgets)
include(../../../../../../../cmake_modules/maidsafe_find_qt5.cmake)
if(NOT AllQt5_FOUND)
  message(STATUS "Didn't find all required modules of Qt5. Not building Network Viewer tool.")
  return()
elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
  message(STATUS "Will not build Network Viewer tool with Clang for now.")
  return()
endif()

set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

project(network_viewer)

include(../../../../../../../cmake_modules/standard_setup.cmake)

# Enable auto-linking qtmain for Windows.
cmake_policy(SET CMP0020 NEW)


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
ms_add_executable(network_viewer "Tools/Routing"
                      ${NETWORK_VIEWER_MAIN}
                      ${ALL_NETWORK_VIEWER_SOURCE_FILES}
                      ${ALL_NETWORK_VIEWER_HEADER_FILES}
                      ${RESOURCE_FILES_NETWORK_VIEWER_RESOURCES_DIR}
                      ${ALL_NETWORK_VIEWER_RESOURCE_SOURCE_FILES})
target_include_directories(network_viewer PRIVATE ${PROJECT_SOURCE_DIR})
set(AllExesForCurrentProject ${AllExesForCurrentProject} PARENT_SCOPE)
if(WIN32)
  set_target_properties(network_viewer PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

target_link_libraries(network_viewer maidsafe_common maidsafe_network_viewer ${Qt5TargetLibs})

rename_outdated_built_exes()


#==================================================================================================#
# Set compiler and linker flags                                                                    #
#==================================================================================================#
include(standard_flags)

target_compile_definitions(network_viewer PRIVATE $<$<CONFIG:Release>:QT_NO_DEBUG_OUTPUT QT_NO_WARNING_OUTPUT QT_NO_DEBUG_STREAM>)

if(WIN32)
  string(REGEX REPLACE "/Zm[0-9]+ " "/Zm500 " CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  set_source_files_properties(${ALL_NETWORK_VIEWER_SOURCE_FILES} PROPERTIES
                              COMPILE_FLAGS "/Yu\"${NETWORK_VIEWER_PCH_HEADER}\" /FI\"${NETWORK_VIEWER_PCH_HEADER}\"")
  set_source_files_properties("${PROJECT_SOURCE_DIR}/helpers/qt_push_headers.cc" PROPERTIES
                              COMPILE_FLAGS "/Yc\"${NETWORK_VIEWER_PCH_HEADER}\" /FI\"${NETWORK_VIEWER_PCH_HEADER}\"")
endif()


#==================================================================================================#
# Tests                                                                                            #
#==================================================================================================#
if(MaidsafeTesting)
  add_style_test()
endif()

message(STATUS "Found Qt5. Network Viewer tool enabled.")
