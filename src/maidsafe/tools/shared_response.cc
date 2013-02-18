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
 * @file  commands.cc
 * @brief Console commands to demo routing stand alone node.
 * @date  2012-10-19
 */

#include "maidsafe/tools/shared_response.h"

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
  for (auto &responsed_node : responded_nodes_) {
    std::cout << "\t" << maidsafe::HexSubstr(responsed_node.string()) << std::endl;
    EXPECT_TRUE(std::find(closest_nodes_.begin(),
                             closest_nodes_.end(),
                             responsed_node) != closest_nodes_.end());
  }
  std::cout<< "Average time taken for receiving msg:"
           <<(average_response_time_ / responded_nodes_.size())
           <<std::endl;
}

void SharedResponse::PrintRoutingTable(std::string response) {
  if (std::string::npos != response.find("request_routing_table")) {
    std::string response_node_list_msg(
        response.substr(response.find("---") + 3,
            response.size() - (response.find("---") + 3)));
    std::vector<NodeId> node_list(
        maidsafe::routing::DeserializeNodeIdList(response_node_list_msg));
    std::cout << "RECEIVED ROUTING TABLE::::" << std::endl;
    for (auto &node_id : node_list)
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
