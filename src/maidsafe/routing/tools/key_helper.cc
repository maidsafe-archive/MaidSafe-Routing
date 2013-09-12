/*  Copyright 2012 MaidSafe.net limited

    This MaidSafe Software is licensed to you under (1) the MaidSafe.net Commercial License,
    version 1.0 or later, or (2) The General Public License (GPL), version 3, depending on which
    licence you accepted on initial access to the Software (the "Licences").

    By contributing code to the MaidSafe Software, or to this project generally, you agree to be
    bound by the terms of the MaidSafe Contributor Agreement, version 1.0, found in the root
    directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also
    available at: http://www.novinet.com/license

    Unless required by applicable law or agreed to in writing, the MaidSafe Software distributed
    under the GPL Licence is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS
    OF ANY KIND, either express or implied.

    See the Licences for the specific language governing permissions and limitations relating to
    use of the MaidSafe Software.                                                                 */

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


namespace {

typedef std::vector<maidsafe::passport::Pmid> PmidVector;

const std::string kHelperVersion = "MaidSafe Routing KeysHelper " + maidsafe::kApplicationVersion();

void PrintKeys(const PmidVector &all_pmids) {
  for (size_t i = 0; i < all_pmids.size(); ++i)
    std::cout << '\t' << i << "\t PMID " << maidsafe::HexSubstr(all_pmids[i].name()->string())
              << (i < 2 ? " (bootstrap)" : "") << std::endl;
}

bool CreateKeys(const size_t &pmids_count, PmidVector &all_pmids) {
  all_pmids.clear();
  for (size_t i = 0; i < pmids_count; ++i) {
    try {
      maidsafe::passport::Anmaid anmaid;
      maidsafe::passport::Maid maid(anmaid);
      maidsafe::passport::Pmid pmid(maid);
      all_pmids.push_back(pmid);
    }
    catch(const std::exception& /*ex*/) {
      LOG(kError) << "CreatePmids - Could not create ID #" << i;
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

}  // unnamed namespace

int main(int argc, char* argv[]) {
  maidsafe::log::Logging::Instance().Initialise(argc, argv);

  std::cout << kHelperVersion << std::endl;

  int result(0);
  boost::system::error_code error_code;

  size_t pmids_count(12);

  try {
    // Options allowed only on command line
    po::options_description generic_options("Commands");
    generic_options.add_options()
        ("help,h", "Print this help message")
        ("create,c", "Create pmids and write to file")
        ("load,l", "Load pmids from file")
        ("delete,d", "Delete pmids file")
        ("print,p", "Print the list of pmids available");

    // Options allowed both on command line and in config file
    po::options_description config_file_options("Configuration options");
    config_file_options.add_options()
        ("pmids_count,n",
            po::value<size_t>(&pmids_count)->default_value(pmids_count),
            "Number of pmids to create")
        ("pmids_path",
            po::value<std::string>()->default_value(
                fs::path(fs::temp_directory_path(error_code) / "pmids_list.dat").string()),
            "Path to pmids file");

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

    PmidVector all_pmids;
    fs::path pmids_path(GetPathFromProgramOption("pmids_path", &variables_map, false, true));

    if (do_create) {
      if (CreateKeys(pmids_count, all_pmids)) {
        std::cout << "Created " << all_pmids.size() << " fobs." << std::endl;
        if (maidsafe::passport::detail::WritePmidList(pmids_path, all_pmids))
          std::cout << "Wrote pmids to " << pmids_path << std::endl;
        else
          std::cout << "Could not write pmids to " << pmids_path << std::endl;
      } else {
        std::cout << "Could not create pmids." << std::endl;
      }
    } else if (do_load) {
      try {
        all_pmids = maidsafe::passport::detail::ReadPmidList(pmids_path);
        std::cout << "Loaded " << all_pmids.size() << " pmids from " << pmids_path << std::endl;
      }
      catch(const std::exception& /*ex*/) {
        all_pmids.clear();
        std::cout << "Could not load fobs from " << pmids_path << std::endl;
      }
    }

    if (do_print)
      PrintKeys(all_pmids);

    if (do_delete) {
      if (fs::remove(pmids_path, error_code))
        std::cout << "Deleted " << pmids_path << std::endl;
      else
        std::cout << "Could not delete " << pmids_path << std::endl;
    }
  }
  catch(const std::exception& exception) {
    std::cout << "Error: " << exception.what() << std::endl;
    result = -2;
  }

  return result;
}
