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
 * @file  commands.h
 * @brief Head File for commands.cc .
 * @date  2012-10-19
 */

#ifndef MAIDSAFE_TOOLS_SHARED_RESPONSE_H_
#define MAIDSAFE_TOOLS_SHARED_RESPONSE_H_

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

#endif  // MAIDSAFE_TOOLS_SHARED_RESPONSE_H_
