/*
* ============================================================================
*
* Copyright [2012] maidsafe.net limited
*
* Description:  Exposing the Routing Generic Node API as a module to Python.
* Version:      1.0
* Created:      2012-12-10
* Revision:     none
* Compiler:     gcc
* Company:      maidsafe.net limited
*
* The following source code is property of maidsafe.net limited and is not
* meant for external use.  The use of this code is governed by the license
* file LICENSE.TXT found in the root of this directory and also on
* www.maidsafe.net.
*
* You are not free to copy, amend or otherwise use this source code without
* the explicit written permission of the board of directors of maidsafe.net.
*
* ============================================================================
*/

#include "boost/filesystem/path.hpp"
#ifdef __MSVC__
#  pragma warning(push)
#  pragma warning(disable: 4100 4127 4244)
#endif
#include "boost/python.hpp"
#include "boost/python/suite/indexing/map_indexing_suite.hpp"
#include "boost/python/suite/indexing/vector_indexing_suite.hpp"
#ifdef __MSVC__
#  pragma warning(pop)
#endif

#include "maidsafe/common/log.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/tools/commands.h"
#include "maidsafe/routing/utils.h"

/**
 * NOTE
 * - set PYTHONPATH to your build directory, or move the module to where Python finds it
 * - in Python, call "from routing_python_api import *"
 */

namespace bpy = boost::python;
namespace rt = maidsafe::routing;

namespace {
  struct PathConverter {
    static PyObject* convert(const boost::filesystem::path& path) {
      return bpy::incref(bpy::str(path.c_str()).ptr());
    }
  };

  struct PathExtractor {
    static void* convertible(PyObject* obj_ptr) {
      return PyString_Check(obj_ptr) ? obj_ptr : nullptr;
    }
    static void construct(PyObject* obj_ptr, bpy::converter::rvalue_from_python_stage1_data* data) {
      const char* value = PyString_AsString(obj_ptr);
      assert(value);
      typedef bpy::converter::rvalue_from_python_storage<boost::filesystem::path> storage_type;
      void* storage = reinterpret_cast<storage_type*>(data)->storage.bytes;
      new(storage) boost::filesystem::path(value);
      data->convertible = storage;
    }
  };

#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Weffc++"
#endif
// BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(create_user_overloads, CreateUser, 3, 4)
// BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(get_contacts_overloads, GetContacts, 1, 2)
// BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(accept_sent_file_overloads, AcceptSentFile, 1, 3)
#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif
}

BOOST_PYTHON_MODULE(routing_python_api) {
//   bpy::to_python_converter<boost::filesystem::path, PathConverter>();
//   bpy::converter::registry::push_back(&PathExtractor::convertible,
//                                       &PathExtractor::construct,
//                                       bpy::type_id<boost::filesystem::path>());

  bpy::class_<rt::test::Commands>(
      "RoutingNode", bpy::init<std::string, bool, int>())
//       .def("Run", &rt::test::Commands::Run)
      .def("SetPeer", &rt::test::Commands::SetPeer)
      .def("Join", &rt::test::Commands::Join)
      .def("ZeroStateJoin", &rt::test::Commands::ZeroStateJoin)
      .def("Joined", &rt::test::Commands::Joined)
      .def("GetNodeInfo", &rt::test::Commands::GetNodeInfo)
      .def("SendAMessage", &rt::test::Commands::SendAMessage)
      .def("ExistInRoutingTable", &rt::test::Commands::ExistInRoutingTable)
      .def("RoutingTableSize", &rt::test::Commands::RoutingTableSize);
}
