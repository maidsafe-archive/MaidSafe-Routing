/***************************************************************************************************
 *  Copyright 2012 MaidSafe.net limited                                                            *
 *                                                                                                 *
 *  The following source code is property of MaidSafe.net limited and is not meant for external    *
 *  use.  The use of this code is governed by the licence file licence.txt found in the root of    *
 *  this directory and also on www.maidsafe.net.                                                   *
 *                                                                                                 *
 *  You are not free to copy, amend or otherwise use this source code without the explicit         *
 *  written permission of the board of directors of MaidSafe.net.                                  *
 **************************************************************************************************/

#include <signal.h>
#include "boost/filesystem.hpp"
#include "boost/program_options.hpp"

#include "maidsafe/common/crypto.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/tools/commands.h"
#include "maidsafe/routing/utils.h"

namespace bptime = boost::posix_time;
namespace fs = boost::filesystem;
namespace po = boost::program_options;
namespace ma = maidsafe::asymm;

struct PortRange {
  PortRange(uint16_t first, uint16_t second)
      : first(first), second(second) {}
  uint16_t first;
  uint16_t second;
};

namespace {

// This function is needed to avoid use of po::bool_switch causing MSVC warning C4505:
// 'boost::program_options::typed_value<bool>::name' : unreferenced local function has been removed.
#ifdef MAIDSAFE_WIN32
void UseUnreferenced() {
  auto dummy = po::typed_value<bool>(nullptr);
  (void)dummy;
}
#endif
void ConflictingOptions(const po::variables_map &variables_map,
                        const char *opt1,
                        const char *opt2) {
  if (variables_map.count(opt1) && !variables_map[opt1].defaulted()
      && variables_map.count(opt2) && !variables_map[opt2].defaulted()) {
    throw std::logic_error(std::string("Conflicting options '") + opt1 +
                           "' and '" + opt2 + "'.");
  }
}

// Function used to check that if 'for_what' is specified, then
// 'required_option' is specified too.
void OptionDependency(const po::variables_map &variables_map,
                      const char *for_what,
                      const char *required_option) {
  if (variables_map.count(for_what) && !variables_map[for_what].defaulted()) {
    if (variables_map.count(required_option) == 0 ||
        variables_map[required_option].defaulted()) {
      throw std::logic_error(std::string("Option '") + for_what
                             + "' requires option '" + required_option + "'.");
    }
  }
}

volatile bool ctrlc_pressed(false);
// reported unused (dirvine)
// void CtrlCHandler(int /*a*/) {
//   ctrlc_pressed = true;
// }

}  // unnamed namespace

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

int main(int argc, char **argv) {
  maidsafe::log::Logging::Instance().Initialise(argc, argv);

  try {
    int identity_index;
    boost::system::error_code error_code;
    po::options_description options_description("Options");
    options_description.add_options()
        ("help,h", "Print options.")
        ("start,s", "Start a node (default as vault)")
        ("client,c", po::bool_switch(), "Start as client (default is vault)")
        ("bootstrap,b", "Start as bootstrap (default is non-bootstrap)")
        ("peer,p", po::value<std::string>()->default_value(""), "Endpoint of bootstrap peer")
        ("identity_index,i", po::value<int>(&identity_index)->default_value(-1),
            "Entry from keys file to use as ID (starts from 0)")
        ("pmids_path",
            po::value<std::string>()->default_value(
                fs::path(fs::temp_directory_path(error_code) / "pmids_list.dat").string()),
            "Path to pmid file");

    po::variables_map variables_map;
//     po::store(po::parse_command_line(argc, argv, options_description),
//               variables_map);
    po::store(po::command_line_parser(argc, argv).options(options_description).allow_unregistered().
                                                  run(), variables_map);
    po::notify(variables_map);

    if (variables_map.count("help") || (!variables_map.count("start"))) {
      std::cout << options_description << std::endl;
      return 0;
    }

    // Load fob list and local fob
    std::vector<maidsafe::passport::Pmid> all_pmids;
    maidsafe::passport::Anmaid anmaid;
    maidsafe::passport::Maid maid(anmaid);
    maidsafe::passport::Pmid local_pmid(maid);
    boost::filesystem::path pmids_path(GetPathFromProgramOption(
        "pmids_path", &variables_map, false, true));
    if (fs::exists(pmids_path, error_code)) {
      all_pmids = maidsafe::passport::detail::ReadPmidList(pmids_path);
      std::cout << "Loaded " << all_pmids.size() << " fobs." << std::endl;
      if (static_cast<uint32_t>(identity_index) >= all_pmids.size() || identity_index < 0) {
        std::cout << "ERROR : index exceeds fob pool -- pool has "
                  << all_pmids.size() << " fobs, while identity_index is "
                  << identity_index << std::endl;
        return 0;
      } else {
        local_pmid = all_pmids[identity_index];
        std::cout << "Using identity #" << identity_index << " from keys file"
                  << " , value is : " << maidsafe::HexSubstr(local_pmid.name().data) << std::endl;
      }
    }

    ConflictingOptions(variables_map, "client", "bootstrap");
    OptionDependency(variables_map, "start", "identity_index");
    OptionDependency(variables_map, "peer", "identity_index");

    // Ensure correct index range is being used
    bool client_only_node(variables_map["client"].as<bool>());
    if (client_only_node) {
      if (identity_index < static_cast<int>(all_pmids.size() / 2)) {
        std::cout << "ERROR : Incorrect identity_index used for a client, must between "
                  << all_pmids.size() / 2 << " and " << all_pmids.size() - 1 << std::endl;
        return 0;
      }
    } else {
      if (identity_index >= static_cast<int>(all_pmids.size() / 2)) {
        std::cout << "ERROR : Incorrect identity_index used for a vault, must between 0 and "
                  << all_pmids.size() / 2 - 1 << std::endl;
        return 0;
      }
    }
    // Initial demo_node
    std::cout << "Creating node..." << std::endl;
    maidsafe::routing::test::NodeInfoAndPrivateKey node_info(
        maidsafe::routing::test::MakeNodeInfoAndKeysWithPmid(local_pmid));
    maidsafe::routing::test::DemoNodePtr demo_node(
        new maidsafe::routing::test::GenericNode(client_only_node, node_info));

    if (variables_map.count("bootstrap")) {
      if (identity_index >= 2) {
        std::cout << "ERROR : trying to use non-bootstrap identity" << std::endl;
        return 0;
      }
      std::cout << "------ Current BootStrap node endpoint info : "
                << demo_node->endpoint() << " ------ " << std::endl;
    }

    maidsafe::routing::test::Commands commands(demo_node, all_pmids, identity_index);
    std::string peer(variables_map.at("peer").as<std::string>());
    if (!peer.empty()) {
      commands.GetPeer(peer);
    }
    commands.Run();

    std::cout << "Node stopped successfully." << std::endl;
  }
  catch(const std::exception &e) {
    std::cout << "Error: " << e.what() << std::endl;
    return -1;
  }
  return 0;
}
