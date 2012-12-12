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
  Commands(std::string fobs_path_str, bool client_only_node, int identity_index);
  void Run();
  void SetPeer(std::string peer);
  void Join();
  void ZeroStateJoin();
  bool Joined() { return demo_node_->joined(); }
  std::string GetNodeInfo() { return node_info_; }

  bool SendAMessage(int identity_index, int destination_type, int data_size);
  bool ExistInRoutingTable(int peer_index);
  int RoutingTableSize();

 private:
  typedef std::vector<std::string> Arguments;

  void InitDemoNode(bool client_only_node, const Fob& this_fob);
  void PrintUsage();
  void ProcessCommand(const std::string &cmdline);
  void MarkResultArrived();
  bool ResultArrived() { return result_arrived_; }
  bool SendAMsg(const int& identity_index, const DestinationType& destination_type,
                std::string &data);
  void PrintRoutingTable();
  void Validate(const NodeId& node_id, GivePublicKeyFunctor give_public_key);

  NodeId CalculateClosests(const NodeId& target_id,
                           std::vector<NodeId>& closests,
                           uint16_t num_of_closests);

  std::shared_ptr<GenericNode> demo_node_;
  std::vector<maidsafe::Fob> all_fobs_;
  std::vector<NodeId> all_ids_;
  int identity_index_;
  std::string node_info_;
  boost::asio::ip::udp::endpoint bootstrap_peer_ep_;
  size_t data_size_;
  bool result_arrived_, finish_;
  std::shared_ptr<std::future<int> > boot_strap_;
  std::vector<NodeId> peer_routing_table_;
  std::shared_ptr<boost::mutex> wait_mutex_;
  std::shared_ptr<boost::condition_variable> wait_cond_var_;
  std::function<void()> mark_results_arrived_;
};

} // namespace test

} // namespace routing

} // namespace maidsafe

#endif // MAIDSAFE_ROUTING_DEMO_COMMANDDS_H_
