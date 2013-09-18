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

#ifndef MAIDSAFE_ROUTING_TOOLS_SHARED_RESPONSE_H_
#define MAIDSAFE_ROUTING_TOOLS_SHARED_RESPONSE_H_

#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "boost/date_time/posix_time/posix_time_types.hpp"
#include "boost/thread/condition_variable.hpp"
#include "boost/thread/mutex.hpp"

#include "maidsafe/routing/tests/test_utils.h"
#include "maidsafe/routing/utils.h"


namespace maidsafe {

namespace routing {

namespace test {
class SharedResponse {
 public:
  SharedResponse(std::vector<NodeId> closest_nodes,
                 uint16_t expect_responses)
  : closest_nodes_(closest_nodes),
    responded_nodes_(),
    expected_responses_(expect_responses),
    msg_send_time_(boost::posix_time::microsec_clock::universal_time()),
    average_response_time_(boost::posix_time::milliseconds(0)),
    mutex_() {}
  ~SharedResponse() {
    // CheckAndPrintResult();
  }
  void CheckAndPrintResult();
  void CollectResponse(std::string response);
  void PrintRoutingTable(std::string response);

  std::vector<NodeId> closest_nodes_;
  std::set<NodeId> responded_nodes_;
  uint32_t expected_responses_;
  boost::posix_time::ptime msg_send_time_;
  boost::posix_time::milliseconds average_response_time_;
  std::mutex mutex_;

 private:
  SharedResponse(const SharedResponse&);
  SharedResponse& operator=(const SharedResponse&);
};

}  //  namespace test

}  //  namespace routing

}  //  namespace maidsafe

#endif  // MAIDSAFE_ROUTING_TOOLS_SHARED_RESPONSE_H_
