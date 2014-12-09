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

#ifndef MAIDSAFE_ROUTING_TOOLS_COMMANDS_H_
#define MAIDSAFE_ROUTING_TOOLS_COMMANDS_H_

#include <atomic>
#include <condition_variable>
#include <memory>
#include <string>
#include <vector>
#include <mutex>

#include "boost/date_time/posix_time/posix_time_types.hpp"

#include "maidsafe/passport/types.h"
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
                    std::vector<maidsafe::passport::detail::AnmaidToPmid> all_keys,
                    int identity_index);
  void Run();
  void GetPeer(const std::string& peer);

 private:
  typedef std::vector<std::string> Arguments;

  void PrintUsage();
  void ProcessCommand(const std::string& cmdline);
  void MarkResultArrived();
  bool ResultArrived() { return result_arrived_; }

  void PrintRoutingTable();
  void ZeroStateJoin();
  void Join();
  void Validate(const Address& Address, GivePublicKeyFunctor give_public_key);
  void SendMessages(int identity_index, const DestinationType& destination_type,
                    bool is_routing_req, int messages_count);

  Address CalculateClosests(const Address& target_id, std::vector<Address>& closests,
                            unsigned int num_of_closests);
  unsigned int MakeMessage(int id_index, const DestinationType& destination_type,
                           std::vector<Address>& closest_nodes, Address& dest_id);

  void CalculateTimeToSleep(std::chrono::milliseconds& msg_sent_time);

  void SendAMessage(std::atomic<int>& successful_count, unsigned int& operation_count,
                    std::mutex& mutex, std::condition_variable& cond_var, int messages_count,
                    unsigned int expect_respondent, std::vector<Address> closest_nodes,
                    Address dest_id, std::string data);
  void PerformanceTest();
  void RunPerformanceTest(bool is_send_group);

  std::shared_ptr<GenericNode> demo_node_;
  std::vector<maidsafe::passport::detail::AnmaidToPmid> all_keys_;
  std::vector<Address> all_ids_;
  int identity_index_;
  boost::asio::ip::udp::endpoint bootstrap_peer_ep_;
  size_t data_size_;
  size_t data_rate_;
  bool result_arrived_, finish_;
  std::mutex wait_mutex_;
  std::condition_variable wait_cond_var_;
  std::function<void()> mark_results_arrived_;
};

}  //  namespace test

}  //  namespace routing

}  //  namespace maidsafe

#endif  // MAIDSAFE_ROUTING_TOOLS_COMMANDS_H_
