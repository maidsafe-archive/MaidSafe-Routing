/***************************************************************************************************
 *  Copyright 2012 maidsafe.net limited                                                            *
 *                                                                                                 *
 *  The following source code is property of maidsafe.net limited and is not meant for external    *
 *  use. The use of this code is governed by the license file LICENSE.TXT found in the root of     *
 *  this directory and also on www.maidsafe.net.                                                   *
 *                                                                                                 *
 *  You are not free to copy, amend or otherwise use this source code without the explicit written *
 *  permission of the board of directors of maidsafe.net.                                          *
 ***********************************************************************************************//**
 * @file  keys_helper.cc
 * @brief Helper program to generate keys and write them to a file.
 * @date  2012-10-19
 */

#include <signal.h>

#include <atomic>
#include <iostream>  // NOLINT
#include <fstream>  // NOLINT
#include <future>  // NOLINT
#include <memory>
#include <string>

#include "boost/filesystem.hpp"
#include "boost/asio.hpp"
#include "boost/program_options.hpp"
#include "boost/thread/condition_variable.hpp"
#include "boost/thread/mutex.hpp"
#include "boost/tokenizer.hpp"
#include "boost/random/mersenne_twister.hpp"
#include "boost/random/uniform_int.hpp"
#include "boost/random/variate_generator.hpp"

#include "maidsafe/common/asio_service.h"
#include "maidsafe/common/config.h"
#include "maidsafe/common/crypto.h"
#include "maidsafe/common/log.h"
#include "maidsafe/common/rsa.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/passport/types.h"

#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/routing_api.h"
#include "maidsafe/routing/utils.h"


namespace fs = boost::filesystem;
namespace po = boost::program_options;
namespace asymm = maidsafe::rsa;

struct std::vector<maidsafe::Fob> FobList;

const std::string kHelperVersion = "MaidSafe Routing KeysHelper " + maidsafe::kApplicationVersion;

boost::mutex mutex_;
boost::condition_variable cond_var_;
bool ctrlc_pressed(false);

void ctrlc_handler(int /*signum*/) {
//   LOG(kInfo) << " Signal received: " << signum;
  {
    boost::mutex::scoped_lock lock(mutex_);
    ctrlc_pressed = true;
  }
  cond_var_.notify_one();
}

void PrintFobs(const FobList &all_fobs) {
  for (size_t i = 0; i < all_fobs.size(); ++i)
    std::cout << '\t' << i << "\t fob : " << maidsafe::HexSubstr(all_fobs[i].identity)
              << (i < 2 ? " (bootstrap)" : "") << std::endl;
}

bool CreateFobs(const size_t &fobs_count, FobList &all_fobs) {
  size_t i(0);
  for (; i < fobs_count / 2; ++i) {
    try {
      all_fobs.push_back(utils::GenerateFob(nullptr));
    }
    catch(const std::exception& /*ex*/) {
      LOG(kError) << "CreateFobs - Could not create ID #" << i;
      return false;
    }
  }
  return true;
}

fs::path GetPathFromProgramOption(const std::string &option_name,
                                  po::variables_map *variables_map,
                                  bool is_dir,
                                  bool create_new_if_absent) {
  fs::path option_path;
  if (variables_map->count(option_name))
    option_path = variables_map->at(option_name).as<std::string>();
  if (option_path.empty())
    return fs::path();

  boost::system::error_code ec;
  if (!fs::exists(option_path, ec) || ec) {
    if (!create_new_if_absent) {
      LOG(kError) << "GetPathFromProgramOption - Invalid " << option_name << ", " << option_path
                  << " doesn't exist or can't be accessed (" << ec.message() << ")";
      return fs::path();
    }

    if (is_dir) {  // Create new dir
      fs::create_directories(option_path, ec);
      if (ec) {
        LOG(kError) << "GetPathFromProgramOption - Unable to create new dir " << option_path << " ("
                    << ec.message() << ")";
        return fs::path();
      }
    } else {  // Create new file
      if (option_path.has_filename()) {
        try {
          std::ofstream ofs(option_path.c_str());
        }
        catch(const std::exception &e) {
          LOG(kError) << "GetPathFromProgramOption - Exception while creating new file: "
                      << e.what();
          return fs::path();
        }
      }
    }
  }

  if (is_dir) {
    if (!fs::is_directory(option_path, ec) || ec) {
      LOG(kError) << "GetPathFromProgramOption - Invalid " << option_name << ", " << option_path
                  << " is not a directory (" << ec.message() << ")";
      return fs::path();
    }
  } else {
    if (!fs::is_regular_file(option_path, ec) || ec) {
      LOG(kError) << "GetPathFromProgramOption - Invalid " << option_name << ", " << option_path
                  << " is not a regular file (" << ec.message() << ")";
      return fs::path();
    }
  }

  LOG(kInfo) << "GetPathFromProgramOption - " << option_name << " is " << option_path;
  return option_path;
}

int main(int argc, char* argv[]) {
  maidsafe::log::Logging::Instance().Initialise(argc, argv);

  std::cout << kHelperVersion << std::endl;

  int result(0);
  boost::system::error_code error_code;

  size_t fobs_count(12);

  try {
    // Options allowed only on command line
    po::options_description generic_options("Commands");
    generic_options.add_options()
        ("help,h", "Print this help message")
        ("create,c", "Create fobs and write to file")
        ("load,l", "Load fobs from file")
        ("delete,d", "Delete fobs file")
        ("print,p", "Print the list of fobs available");

    // Options allowed both on command line and in config file
    po::options_description config_file_options("Configuration options");
    config_file_options.add_options()
        ("fobs_count,n",
            po::value<size_t>(&fobs_count)->default_value(fobs_count),
            "Number of fobs to create")
        ("fobs_path",
            po::value<std::string>()->default_value(
                fs::path(fs::temp_directory_path(error_code) / "fob_list.dat").string()),
            "Path to fobs file");

    po::options_description cmdline_options;
    cmdline_options.add(generic_options).add(config_file_options);

    po::variables_map variables_map;
    po::store(po::command_line_parser(argc, argv).options(cmdline_options).allow_unregistered().
                                                  run(), variables_map);
    po::notify(variables_map);

    bool do_create(variables_map.count("create") != 0);
    bool do_load(variables_map.count("load") != 0);
    bool do_delete(variables_map.count("delete") != 0);
    bool do_print(variables_map.count("print") != 0);

    if (variables_map.count("help") ||
        (!do_create && !do_load && !do_delete && !do_print)) {
      std::cout << cmdline_options << std::endl
                << "Commands are executed in this order: [c|l] p d" << std::endl;
      return 0;
    }

    FobList all_fobs;
    fs::path fobs_path(GetPathFromProgramOption("fobs_path", &variables_map, false, true));

    if (do_create) {
      if (CreateFobs(fobs_count, all_fobs)) {
        std::cout << "Created " << all_fobs.size() << " fobs." << std::endl;
        if (maidsafe::routing::WriteFobList(fobs_path, all_fobs))
          std::cout << "Wrote fobs to " << fobs_path << std::endl;
        else
          std::cout << "Could not write fobs to " << fobs_path << std::endl;
      } else {
        std::cout << "Could not create fobs." << std::endl;
      }
    } else if (do_load) {
      try {
        all_fobs = maidsafe::routing::ReadFobList(fobs_path);
        std::cout << "Loaded " << all_fobs.size() << " fobs from " << fobs_path << std::endl;
      }
      catch(const std::exception& /*ex*/) {
        all_fobs.clear();
        std::cout << "Could not load fobs from " << fobs_path << std::endl;
      }
    }

    if (do_print)
      PrintFobs(all_fobs);

    if (do_delete) {
      if (fs::remove(fobs_path, error_code))
        std::cout << "Deleted " << fobs_path << std::endl;
      else
        std::cout << "Could not delete " << fobs_path << std::endl;
    }
  }
  catch(const std::exception& exception) {
    std::cout << "Error: " << exception.what() << std::endl;
    result = -2;
  }

  return result;
}
