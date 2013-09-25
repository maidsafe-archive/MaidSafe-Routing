/*  Copyright 2012 MaidSafe.net limited

    This MaidSafe Software is licensed to you under (1) the MaidSafe.net Commercial License,
    version 1.0 or later, or (2) The General Public License (GPL), version 3, depending on which
    licence you accepted on initial access to the Software (the "Licences").

    By contributing code to the MaidSafe Software, or to this project generally, you agree to be
    bound by the terms of the MaidSafe Contributor Agreement, version 1.0, found in the root
    directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also
    available at: http://www.maidsafe.net/licenses

    Unless required by applicable law or agreed to in writing, the MaidSafe Software distributed
    under the GPL Licence is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS
    OF ANY KIND, either express or implied.

    See the Licences for the specific language governing permissions and limitations relating to
    use of the MaidSafe Software.                                                                 */

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
  PortRange(uint16_t first, uint16_t second) : first(first), second(second) {}
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
void ConflictingOptions(const po::variables_map& variables_map, const char* opt1,
                        const char* opt2) {
  if (variables_map.count(opt1) && !variables_map[opt1].defaulted() && variables_map.count(opt2) &&
      !variables_map[opt2].defaulted()) {
    throw std::logic_error(std::string("Conflicting options '") + opt1 + "' and '" + opt2 + "'.");
  }
}

// Function used to check that if 'for_what' is specified, then
// 'required_option' is specified too.
void OptionDependency(const po::variables_map& variables_map, const char* for_what,
                      const char* required_option) {
  if (variables_map.count(for_what) && !variables_map[for_what].defaulted()) {
    if (variables_map.count(required_option) == 0 || variables_map[required_option].defaulted()) {
      throw std::logic_error(std::string("Option '") + for_what + "' requires option '" +
                             required_option + "'.");
    }
  }
}

// volatile bool ctrlc_pressed(false);
//  reported unused (dirvine)
//  void CtrlCHandler(int /*a*/) {
//    ctrlc_pressed = true;
//  }

}  // unnamed namespace

int main(int argc, char** argv) {
  maidsafe::log::Logging::Instance().Initialise(argc, argv);

  try {
    int identity_index;
    boost::system::error_code error_code;
    po::options_description options_description("Options");
    options_description.add_options()("help,h", "Print options.")(
        "start,s", "Start a node (default as vault)")("client,c", po::bool_switch(),
                                                      "Start as client (default is vault)")(
        "bootstrap,b", "Start as bootstrap (default is non-bootstrap)")(
        "peer,p", po::value<std::string>()->default_value(""), "Endpoint of bootstrap peer")(
        "identity_index,i", po::value<int>(&identity_index)->default_value(-1),
        "Entry from keys file to use as ID (starts from 0)")(
        "pmids_path", po::value<std::string>()->default_value(fs::path(
                          fs::temp_directory_path(error_code) / "pmids_list.dat").string()),
        "Path to pmid file");

    po::variables_map variables_map;
    //     po::store(po::parse_command_line(argc, argv, options_description),
    //               variables_map);
    po::store(
        po::command_line_parser(argc, argv).options(options_description).allow_unregistered().run(),
        variables_map);
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
    auto pmids_path(maidsafe::GetPathFromProgramOptions("pmids_path", variables_map, false, true));
    if (fs::exists(pmids_path, error_code)) {
      all_pmids = maidsafe::passport::detail::ReadPmidList(pmids_path);
      std::cout << "Loaded " << all_pmids.size() << " fobs." << std::endl;
      if (static_cast<uint32_t>(identity_index) >= all_pmids.size() || identity_index < 0) {
        std::cout << "ERROR : index exceeds fob pool -- pool has " << all_pmids.size()
                  << " fobs, while identity_index is " << identity_index << std::endl;
        return 0;
      } else {
        local_pmid = all_pmids[identity_index];
        std::cout << "Using identity #" << identity_index << " from keys file"
                  << " , value is : " << maidsafe::HexSubstr(local_pmid.name().value) << std::endl;
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
      std::cout << "------ Current BootStrap node endpoint info : " << demo_node->endpoint()
                << " ------ " << std::endl;
    }

    maidsafe::routing::test::Commands commands(demo_node, all_pmids, identity_index);
    std::string peer(variables_map.at("peer").as<std::string>());
    if (!peer.empty()) {
      commands.GetPeer(peer);
    }
    commands.Run();

    std::cout << "Node stopped successfully." << std::endl;
  }
  catch (const std::exception& e) {
    std::cout << "Error: " << e.what() << std::endl;
    return -1;
  }
  return 0;
}
