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

#ifndef MAIDSAFE_DHT_DEMO_COMMANDS_H_
#define MAIDSAFE_DHT_DEMO_COMMANDS_H_

#include <memory>
#include <string>
#include <vector>

#include "boost/date_time/posix_time/posix_time_types.hpp"
#include "boost/thread/condition_variable.hpp"
#include "boost/thread/mutex.hpp"

#include "maidsafe/routing/tests/routing_network.h"
#include "maidsafe/routing/tests/test_utils.h"
#include "maidsafe/routing/utils.h"

namespace bptime = boost::posix_time;

namespace maidsafe {

namespace routing {

namespace test {

typedef std::shared_ptr<GenericNode> DemoNodePtr;

class Commands {
 public:
  explicit Commands(DemoNodePtr demo_node,
                    std::vector<maidsafe::Fob> all_fobs,
                    int identity_index);
  void Run();
  void GetPeer(const std::string &peer);

 private:
  typedef std::vector<std::string> Arguments;

  void PrintUsage();
  void ProcessCommand(const std::string &cmdline);
  void MarkResultArrived();
  bool ResultArrived() { return result_arrived_; }

  void PrintRoutingTable();
  void ZeroStateJoin();
  void Join();
  void Validate(const NodeId& node_id, GivePublicKeyFunctor give_public_key);
  void SendAMsg(const int& identity_index, const DestinationType& destination_type,
                const std::string &data);

  NodeId CalculateClosests(const NodeId& target_id,
                           std::vector<NodeId>& closests,
                           uint16_t num_of_closests);

  std::shared_ptr<GenericNode> demo_node_;
  std::vector<maidsafe::Fob> all_fobs_;
  std::vector<NodeId> all_ids_;
  int identity_index_;
  boost::asio::ip::udp::endpoint bootstrap_peer_ep_;
  size_t data_size_;
  bool result_arrived_, finish_;
  boost::mutex wait_mutex_;
  boost::condition_variable wait_cond_var_;
  std::function<void()> mark_results_arrived_;
};

} // namespace test

} // namespace routing

} // namespace maidsafe

#endif // MAIDSAFE_ROUTING_DEMO_COMMANDDS_H_
