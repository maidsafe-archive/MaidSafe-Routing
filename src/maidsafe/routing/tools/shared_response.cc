/* Copyright 2012 MaidSafe.net limited

This MaidSafe Software is licensed under the MaidSafe.net Commercial License, version 1.0 or later,
and The General Public License (GPL), version 3. By contributing code to this project You agree to
the terms laid out in the MaidSafe Contributor Agreement, version 1.0, found in the root directory
of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also available at:

http://www.novinet.com/license

Unless required by applicable law or agreed to in writing, software distributed under the License is
distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
implied. See the License for the specific language governing permissions and limitations under the
License.
*/

#include "maidsafe/routing/tools/shared_response.h"

#include <iostream> // NOLINT

#include "boost/format.hpp"
#ifdef __MSVC__
# pragma warning(push)
# pragma warning(disable: 4127)
#endif
#include "boost/tokenizer.hpp"
#ifdef __MSVC__
# pragma warning(pop)
#endif
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
    EXPECT_TRUE(std::find(closest_nodes_.begin(),
                             closest_nodes_.end(),
                             responsed_node) != closest_nodes_.end());
  }
  std::cout << "Average time taken for receiving msg:"
            << (average_response_time_.total_milliseconds() / responded_nodes_.size()) << std::endl;
}

void SharedResponse::PrintRoutingTable(std::string response) {
  if (std::string::npos != response.find("request_routing_table")) {
    std::string response_node_list_msg(
        response.substr(response.find("---") + 3,
            response.size() - (response.find("---") + 3)));
    std::vector<NodeId> node_list(
        maidsafe::routing::DeserializeNodeIdList(response_node_list_msg));
    std::cout << "RECEIVED ROUTING TABLE::::" << std::endl;
    for (const auto& node_id : node_list)
      std::cout << "\t" << maidsafe::HexSubstr(node_id.string()) << std::endl;
  }
}

void SharedResponse::CollectResponse(std::string response) {
  std::lock_guard<std::mutex> lock(mutex_);
  boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
  std::string response_id(response.substr(response.find("+++") + 3, 64));
  responded_nodes_.insert(NodeId(response_id));
  average_response_time_ += (now - msg_send_time_);
  std::cout << "Response received in "
            << now - msg_send_time_ << std::endl;
}

}  //  namespace test

}  //  namespace routing

}  //  namespace maidsafe
