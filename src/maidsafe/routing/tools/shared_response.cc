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

#include "maidsafe/routing/tools/shared_response.h"

#include <iostream>  // NOLINT

#include "boost/format.hpp"
#include "boost/tokenizer.hpp"
#include "boost/lexical_cast.hpp"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

namespace maidsafe {

namespace routing {

namespace test {

void SharedResponse::CheckAndPrintResult() {
  if (responded_nodes_.empty())
    return;

  std::cout << "Received response from following nodes :" << std::endl;
  for (const auto& responsed_node : responded_nodes_) {
    std::cout << "\t" << maidsafe::HexSubstr(responsed_node.string()) << std::endl;
    EXPECT_TRUE(std::find(closest_nodes_.begin(), closest_nodes_.end(), responsed_node) !=
                closest_nodes_.end());
  }
  std::cout << "Average time taken for receiving msg:"
            << (average_response_time_.total_milliseconds() / responded_nodes_.size())
            << " milliseconds" << std::endl;
}

void SharedResponse::PrintRoutingTable(std::string response) {
  if (std::string::npos != response.find("request_routing_table")) {
    std::string response_node_list_msg(
        response.substr(response.find("---") + 3, response.size() - (response.find("---") + 3)));
    std::vector<Address> node_list(
        maidsafe::routing::DeserializeNodeIdList(response_node_list_msg));
    std::cout << "RECEIVED ROUTING TABLE::::" << std::endl;
    for (const auto& Address : node_list)
      std::cout << "\t" << maidsafe::HexSubstr(Address.string()) << std::endl;
  }
}

bool SharedResponse::CollectResponse(std::string response, bool print_performance) {
  std::lock_guard<std::mutex> lock(mutex_);
  boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
  std::string response_id(response.substr(response.find("+++") + 3, 64));
  //   std::cout << "Response with size of " << response.size()
  //             << " bytes received in " << now - msg_send_time_ << " seconds" << std::endl;
  std::cout << "a message from " << Address(response_id);
  auto duration((now - msg_send_time_).total_milliseconds());
  if (duration < Parameters::default_response_timeout.count()) {
    if (responded_nodes_.find(Address(response_id)) != std::end(responded_nodes_)) {
      std::cout << "Wrong message from " << Address(response_id);
      return false;
    }
    responded_nodes_.insert(Address(response_id));
    average_response_time_ += (now - msg_send_time_);
    if (print_performance) {
      double rate(static_cast<double>(response.size() * 2) / duration);
      std::cout << "Direct message sent to " << DebugId(Address(response_id)) << " completed in "
                << duration << " milliseconds, has throughput rate " << rate
                << " kBytes/s when data_size is " << response.size() << " Bytes" << std::endl;
    }
  } else {
    std::cout << "timed out ( " << duration / 1000 << " s) to " << DebugId(Address(response_id))
              << " when data_size is " << response.size() << std::endl;
  }
  return true;
}

void SharedResponse::PrintGroupPerformance(int data_size) {
  if (responded_nodes_.size() < routing::Parameters::group_size) {
    std::cout << "Only received " << responded_nodes_.size() << " responses for a group msg of "
              << data_size << " Bytes" << std::endl;
    return;
  }
  auto duration(average_response_time_.total_milliseconds() / responded_nodes_.size());
  double rate((static_cast<double>(data_size * 2) / duration));
  std::cout << " completed in " << duration << " milliseconds, has throughput rate " << rate
            << " kBytes/s when data_size is " << data_size << " Bytes" << std::endl;
}

}  //  namespace test

}  //  namespace routing

}  //  namespace maidsafe
