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

#include "maidsafe/routing/tools/commands.h"

#include <algorithm>
#include <iostream>  // NOLINT

#include "boost/format.hpp"
#include "boost/filesystem.hpp"
#include "boost/tokenizer.hpp"
#include "boost/lexical_cast.hpp"
#include "maidsafe/common/crypto.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/routing/tools/shared_response.h"

namespace fs = boost::filesystem;

namespace maidsafe {

namespace routing {

namespace test {

Commands::Commands(DemoNodePtr demo_node,
                   std::vector<maidsafe::passport::detail::AnmaidToPmid> all_keys,
                   int identity_index)
    : demo_node_(demo_node), all_keys_(std::move(all_keys)), all_ids_(),
      identity_index_(identity_index), bootstrap_peer_ep_(), data_size_(1024 * 1024),
      data_rate_(1024 * 1024), result_arrived_(false), finish_(false), wait_mutex_(),
      wait_cond_var_(), mark_results_arrived_() {
  // CalculateClosests will only use all_ids_ to calculate expected respondents
  // here it is assumed that the first half of fobs will be used as vault
  // and the latter half part will be used as client, which shall not respond msg
  // i.e. shall not be put into all_ids_
  for (size_t i(0); i < (all_keys_.size() / 2); ++i)
    all_ids_.push_back(NodeId(all_keys_[i].pmid.name().value));

  demo_node->functors_.request_public_key = [this](
      const NodeId & node_id,
      GivePublicKeyFunctor give_public_key) { this->Validate(node_id, give_public_key); };  // NOLINT
  demo_node->functors_.message_and_caching.message_received = [this](
      const std::string& wrapped_message, const ReplyFunctor& reply_functor) {
    std::string reply_msg(wrapped_message + "+++" + demo_node_->node_id().string());
    if (std::string::npos != wrapped_message.find("request_routing_table"))
      reply_msg = reply_msg + "---" + demo_node_->SerializeRoutingTable();
    reply_functor(reply_msg);
  };
  mark_results_arrived_ = std::bind(&Commands::MarkResultArrived, this);
}

void Commands::Validate(const NodeId& node_id, GivePublicKeyFunctor give_public_key) {
  if (node_id == NodeId())
    return;

  auto iter(all_keys_.begin());
  bool find(false);
  while ((iter != all_keys_.end()) && !find) {
    if (iter->pmid.name()->string() == node_id.string())
      find = true;
    else
      ++iter;
  }
  if (iter != all_keys_.end())
    give_public_key((*iter).pmid.public_key());
}

void Commands::Run() {
  PrintUsage();

  if ((!demo_node_->joined()) && (identity_index_ >= 2)) {  // &&
    // (bootstrap_peer_ep_ != boost::asio::ip::udp::endpoint())) {
    // All parameters have been setup via cmdline directly, join the node immediately
    std::cout << "Joining the node ......" << std::endl;
    Join();
  }

  while (!finish_) {
    std::cout << std::endl << std::endl << "Enter command > ";
    std::string cmdline;
    std::getline(std::cin, cmdline);
    {
      std::unique_lock<std::mutex> lock(wait_mutex_);
      ProcessCommand(cmdline);
      wait_cond_var_.wait(lock, boost::bind(&Commands::ResultArrived, this));
      result_arrived_ = false;
    }
  }
}

void Commands::PrintRoutingTable() {
  auto routing_nodes = demo_node_->ReturnRoutingTable();
  std::cout << "ROUTING TABLE::::" << std::endl;
  for (const auto& routing_node : routing_nodes)
    std::cout << "\t" << maidsafe::HexSubstr(routing_node.string()) << std::endl;
}

void Commands::GetPeer(const std::string& peer) {
  size_t delim = peer.rfind(':');
  try {
    bootstrap_peer_ep_.port(static_cast<uint16_t>(atoi(peer.substr(delim + 1).c_str())));
    bootstrap_peer_ep_.address(boost::asio::ip::address::from_string(peer.substr(0, delim)));
    std::cout << "Going to bootstrap from endpoint " << bootstrap_peer_ep_ << std::endl;
  }
  catch (...) {
    std::cout << "Could not parse IPv4 peer endpoint from " << peer << std::endl;
  }
  auto bootstrap_file_path(detail::GetOverrideBootstrapFilePath<false>());
  boost::filesystem::remove(bootstrap_file_path);
  try {
    WriteBootstrapContacts(BootstrapContacts {demo_node_->endpoint(), bootstrap_peer_ep_},
                           bootstrap_file_path);
  } catch (const std::exception& /*error*/) {}  // File updated by peer zerostate node
}

void Commands::ZeroStateJoin() {
  if (demo_node_->joined()) {
    std::cout << "Current node already joined" << std::endl;
    return;
  }
  if (identity_index_ > 1) {
    std::cout << "can't exec ZeroStateJoin as a non-bootstrap node" << std::endl;
    return;
  }

  NodeInfo peer_node_info;
  if (identity_index_ == 0) {
    peer_node_info.id = NodeId(all_keys_[1].pmid.name().value);
    peer_node_info.public_key = all_keys_[1].pmid.public_key();
  } else {
    peer_node_info.id = NodeId(all_keys_[0].pmid.name().value);
    peer_node_info.public_key = all_keys_[0].pmid.public_key();
  }
  peer_node_info.connection_id = peer_node_info.id;

  ReturnCode ret_code(
      static_cast<ReturnCode>(demo_node_->ZeroStateJoin(bootstrap_peer_ep_, peer_node_info)));
  EXPECT_EQ(kSuccess, ret_code);
  if (ret_code == kSuccess)
    demo_node_->set_joined(ret_code == kSuccess);
}

void Commands::SendMessages(int id_index, const DestinationType& destination_type,
                            bool is_routing_req, int messages_count) {
  std::string data, data_to_send;
  //  Check message type
  if (is_routing_req)
    data = "request_routing_table";
  else
    data_to_send = data = RandomAlphaNumericString(data_size_);
  std::chrono::milliseconds msg_sent_time(0);
  CalculateTimeToSleep(msg_sent_time);

  bool infinite(false);
  if (messages_count == -1)
    infinite = true;

  uint32_t message_id(0);
  unsigned int expect_respondent(0);
  std::atomic<int> successful_count(0);
  std::mutex mutex;
  std::condition_variable cond_var;
  unsigned int operation_count(0);
  //   Send messages
  auto timeout(Parameters::default_response_timeout);
  std::cout << "message_count " << messages_count << std::endl;
  if (messages_count * ((destination_type != DestinationType::kGroup) ? 1 : 4) > 10)
    Parameters::default_response_timeout *=
        (messages_count * ((destination_type != DestinationType::kGroup) ? 1 : 4));
  for (int index = 0; index < messages_count || infinite; ++index) {
    std::vector<NodeId> closest_nodes;
    NodeId dest_id;
    expect_respondent = MakeMessage(id_index, destination_type, closest_nodes, dest_id);
    if (expect_respondent == 0)
      return;
    auto start = std::chrono::steady_clock::now();
    data = ">:< " + std::to_string(++message_id) + " <:>" + data;
    SendAMessage(successful_count, operation_count, mutex, cond_var, messages_count,
                 expect_respondent, closest_nodes, dest_id, data);

    data = data_to_send;
    auto now = std::chrono::steady_clock::now();
    Sleep(msg_sent_time - (now - start));
  }
  {
    std::unique_lock<std::mutex> lock(mutex);
    if (operation_count != (messages_count * expect_respondent))
      cond_var.wait(lock);
  }
  std::cout << "Succcessfully received messages count::" << successful_count << std::endl;
  std::cout << "Unsucccessfully received messages count::" << (messages_count - successful_count)
            << std::endl;
  Parameters::default_response_timeout = timeout;
}

unsigned int Commands::MakeMessage(int id_index, const DestinationType& destination_type,
                               std::vector<NodeId>& closest_nodes, NodeId& dest_id) {
  int identity_index;
  if (id_index >= 0)
    identity_index = id_index;
  else
    identity_index = RandomUint32() % (all_keys_.size() / 2);

  if ((identity_index >= 0) && (static_cast<uint32_t>(identity_index) >= all_keys_.size())) {
    std::cout << "ERROR : destination index out of range" << std::endl;
    return 0;
  }
  if (identity_index >= 0) {
    if (destination_type == DestinationType::kGroup)
      dest_id = NodeId(RandomString(NodeId::kSize));
    else
      dest_id = NodeId(all_keys_[identity_index].pmid.name().value);
  }
  std::cout << "Sending a msg from : " << maidsafe::HexSubstr(demo_node_->node_id().string())
            << " to " << (destination_type != DestinationType::kGroup ? ": " : "group : ")
            << maidsafe::HexSubstr(dest_id.string())
            << " , expect receive response from :" << std::endl;
  unsigned int expected_respodents(destination_type != DestinationType::kGroup ? 1 : 4);
  std::vector<NodeId> closests;
  if (destination_type == DestinationType::kGroup)
    NodeId farthest_closests(CalculateClosests(dest_id, closests, expected_respodents));
  else
    closests.push_back(dest_id);
  for (const auto& node_id : closests)
    std::cout << "\t" << maidsafe::HexSubstr(node_id.string()) << std::endl;
  closest_nodes = closests;
  return expected_respodents;
}

void Commands::CalculateTimeToSleep(std::chrono::milliseconds& msg_sent_time) {
  size_t num_msgs_per_second = data_rate_ / data_size_;
  msg_sent_time = std::chrono::milliseconds(1000 / num_msgs_per_second);
}

void Commands::SendAMessage(std::atomic<int>& successful_count, unsigned int& operation_count,
                            std::mutex& mutex, std::condition_variable& cond_var,
                            int messages_count, unsigned int expect_respondent,
                            std::vector<NodeId> closest_nodes, NodeId dest_id, std::string data) {
  bool group_performance(false);
  if ((expect_respondent > 1) && (closest_nodes.empty()))
    group_performance = true;
  auto data_size(data.size());
  auto shared_response_ptr = std::make_shared<SharedResponse>(closest_nodes, expect_respondent);
  auto callable = [shared_response_ptr, &successful_count, &operation_count, &mutex,
                   messages_count, expect_respondent, &cond_var, group_performance,
                   data_size, this](std::string response) {
    if (!response.empty()) {
      std::string string(response.substr(0, 20));
      std::cout << "Received message: " << string << "\n";
      if (!shared_response_ptr->CollectResponse(response, !group_performance))
        return;
      if (shared_response_ptr->expected_responses_ == 1)
        shared_response_ptr->PrintRoutingTable(response);
      if (shared_response_ptr->responded_nodes_.size() ==
          shared_response_ptr->closest_nodes_.size()) {
        shared_response_ptr->CheckAndPrintResult();
        ++successful_count;
      }
    } else {
      std::cout << "Error Response received in "
                << boost::posix_time::microsec_clock::universal_time() -
                       shared_response_ptr->msg_send_time_ << std::endl;
    }
    {
      std::lock_guard<std::mutex> lock(mutex);
      ++operation_count;
      if (operation_count == (messages_count * expect_respondent)) {
        if (group_performance)
          shared_response_ptr->PrintGroupPerformance(static_cast<int>(data_size));
        cond_var.notify_one();
      }
    }
  };
  //  Send the msg
  if (expect_respondent == 1) {
    demo_node_->SendDirect(dest_id, data, false, callable);
  } else {
    if (group_performance)
      std::cout << "Group message sent to " << DebugId(dest_id);
    demo_node_->SendGroup(dest_id, data, false, callable);
  }
}

void Commands::Join() {
  if (demo_node_->joined()) {
    std::cout << "Current node already joined" << std::endl;
    return;
  }
  std::condition_variable cond_var;
  std::mutex mutex;

  std::weak_ptr<GenericNode> weak_node(demo_node_);
  demo_node_->functors_.network_status = [this, &cond_var, weak_node](const int& result) {
    if (std::shared_ptr<GenericNode> node = weak_node.lock()) {
      ASSERT_GE(result, kSuccess);
      if (result == static_cast<int>(node->expected()) && !node->joined()) {
        node->set_joined(true);
        cond_var.notify_one();
      } else {
        std::cout << "Network Status Changed" << std::endl;
        this->PrintRoutingTable();
      }
    }
  };

  demo_node_->Join();

  if (!demo_node_->joined()) {
    std::unique_lock<std::mutex> lock(mutex);
    auto result = cond_var.wait_for(lock, std::chrono::seconds(20));
    EXPECT_EQ(result, std::cv_status::no_timeout);
    Sleep(std::chrono::milliseconds(600));
  }
  std::cout << "Current Node joined, following is the routing table :" << std::endl;
  PrintRoutingTable();
}

void Commands::PrintUsage() {
  std::cout << "\thelp Print options.\n";
  std::cout << "\tpeer <endpoint> Set BootStrap peer endpoint.\n";
  std::cout << "\tzerostatejoin ZeroStateJoin.\n";
  std::cout << "\tjoin Normal Join.\n";
  std::cout << "\tprt Print Local Routing Table.\n";
  std::cout << "\trrt <dest_index> Request Routing Table from peer node with the specified"
            << " identity-index.\n";
  std::cout << "\tsenddirect <dest_index> <num_msg> Send a msg to a node with specified"
            << "  identity-index. -1 for infinite (Default 1)\n";
  std::cout << "\tsendgroup <dest_index> Send a msg to group (default is Random GroupId,"
            << " dest_index for using existing identity as a group_id)\n";
  std::cout << "\tsendmultiple <num_msg> Send num of msg to randomly picked-up destination."
            << " -1 for infinite (Default 10)\n";
  std::cout << "\tsendgroupmultiple <num_msg> Send num of group msg to randomly "
            << " picked-up destination. -1 for infinite\n";
  std::cout << "\tdatasize <data_size> Set the data_size for the message.\n";
  std::cout << "\tdatarate <data_rate> Set the data_rate for the message.\n";
  std::cout << "\tattype Print the NatType of this node.\n";
  std::cout << "\tperformance Execute performance test from this node.\n";
  std::cout << "\texit Exit application.\n";
}

void Commands::ProcessCommand(const std::string& cmdline) {
  if (cmdline.empty()) {
    demo_node_->PostTaskToAsioService(mark_results_arrived_);
    return;
  }

  std::string cmd;
  Arguments args;
  try {
    boost::char_separator<char> sep(" ");
    boost::tokenizer<boost::char_separator<char>> tok(cmdline, sep);
    for (auto it = tok.begin(); it != tok.end(); ++it) {
      if (it == tok.begin())
        cmd = *it;
      else
        args.push_back(*it);
    }
  }
  catch (const std::exception& e) {
    LOG(kError) << "Error processing command: " << e.what();
  }

  if (cmd == "help") {
    PrintUsage();
  } else if (cmd == "prt") {
    PrintRoutingTable();
  } else if (cmd == "rrt") {
    if (args.size() == 1) {
      SendMessages(atoi(args[0].c_str()), DestinationType::kDirect, true, 1);
    } else {
      std::cout << "Error : Try correct option" << std::endl;
    }
  } else if (cmd == "peer") {
    if (args.size() == 1)
      GetPeer(args[0]);
    else
      std::cout << "Error : Try correct option" << std::endl;
  } else if (cmd == "zerostatejoin") {
    ZeroStateJoin();
  } else if (cmd == "join") {
    Join();
  } else if (cmd == "senddirect") {
    if (args.size() == 1) {
      SendMessages(atoi(args[0].c_str()), DestinationType::kDirect, false, 1);
    } else if (args.size() == 2) {
      int count(atoi(args[1].c_str()));
      bool infinite(count < 0);
      if (infinite) {
        std::cout << " Running infinite messaging test. press Ctrl + C to terminate the program"
                  << std::endl;
        SendMessages(atoi(args[0].c_str()), DestinationType::kDirect, false, -1);
      } else {
        SendMessages(atoi(args[0].c_str()), DestinationType::kDirect, false, count);
      }
    }
  } else if (cmd == "sendgroup") {
    if (args.empty())
      SendMessages(-1, DestinationType::kGroup, false, 1);
    else
      SendMessages(atoi(args[0].c_str()), DestinationType::kGroup, false, 1);
  } else if (cmd == "sendgroupmultiple") {
    if (args.size() == 1) {
      SendMessages(-1, DestinationType::kGroup, false, atoi(args[0].c_str()));
    }
  } else if (cmd == "sendmultiple") {
    int num_msg(10);
    if (!args.empty())
      num_msg = atoi(args[0].c_str());
    if (num_msg == -1) {
      std::cout << " Running infinite messaging test. press Ctrl + C to terminate the program"
                << std::endl;
      SendMessages(-1, DestinationType::kDirect, false, -1);
    } else {
      SendMessages(-1, DestinationType::kDirect, false, num_msg);
    }
    boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
    std::cout << "Sent " << num_msg << " messages to randomly picked-up targets. Finished in :"
              << boost::posix_time::microsec_clock::universal_time() - now << std::endl;
  } else if (cmd == "datasize") {
    if (args.size() == 1)
      data_size_ = atoi(args[0].c_str());
    else
      std::cout << "Error : Try correct option" << std::endl;
  } else if (cmd == "datarate") {
    if (args.size() == 1)
      data_rate_ = atoi(args[0].c_str());
    else
      std::cout << "Error : Try correct option" << std::endl;
  } else if (cmd == "nattype") {
    std::cout << "NatType for this node is : " << demo_node_->nat_type() << std::endl;
  } else if (cmd == "performance") {
    PerformanceTest();
  } else if (cmd == "exit") {
    std::cout << "Exiting application...\n";
    finish_ = true;
  } else {
    std::cout << "Invalid command : " << cmd << std::endl;
    PrintUsage();
  }
  demo_node_->PostTaskToAsioService(mark_results_arrived_);
}

void Commands::MarkResultArrived() {
  {
    std::lock_guard<std::mutex> lock(wait_mutex_);
    result_arrived_ = true;
  }
  wait_cond_var_.notify_one();
}

NodeId Commands::CalculateClosests(const NodeId& target_id, std::vector<NodeId>& closests,
                                   unsigned int num_of_closests) {
  if (all_ids_.size() <= num_of_closests) {
    closests = all_ids_;
    return closests[closests.size() - 1];
  }
  std::sort(all_ids_.begin(), all_ids_.end(), [&](const NodeId & lhs, const NodeId & rhs) {
    return NodeId::CloserToTarget(lhs, rhs, target_id);
  });
  closests = std::vector<NodeId>(
      all_ids_.begin() + boost::lexical_cast<bool>(all_ids_[0] == target_id),
      all_ids_.begin() + num_of_closests + boost::lexical_cast<bool>(all_ids_[0] == target_id));
  return closests[closests.size() - 1];
}

void Commands::PerformanceTest() {
  std::cout << "*************  Performance Test Sending Direct Message *************" << std::endl;
  RunPerformanceTest(false);
  std::cout << "*************  Performance Test Sending Group Message *************" << std::endl;
  RunPerformanceTest(true);
}

void Commands::RunPerformanceTest(bool is_send_group) {
  data_size_ = 1;
  int iteration(1);
  uint32_t message_id(0);
  unsigned int expect_respondent(is_send_group ? routing::Parameters::group_size : 1);
  std::vector<NodeId> closest_nodes;
  while (data_size_ < ((1024 * 1024) + 1024)) {
    std::string data, data_to_send;
    data_to_send = data = RandomAlphaNumericString(data_size_);

    auto routing_nodes = demo_node_->ReturnRoutingTable();
    for (const auto& routing_node : routing_nodes) {
      std::atomic<int> successful_count(0);
      std::mutex mutex;
      std::condition_variable cond_var;
      unsigned int operation_count(0);
      data = ">:<" + std::to_string(++message_id) + "<:>" + data;
      SendAMessage(successful_count, operation_count, mutex, cond_var, 1,
                   expect_respondent, closest_nodes, routing_node, data);
      data = data_to_send;  // remove the message_id part
      {
        std::unique_lock<std::mutex> lock(mutex);
        // shall setup a timed out here ?
        if (operation_count != expect_respondent)
          cond_var.wait(lock);
      }
    }
    data_size_ = 1000 * iteration;
    iteration *= 2;
  }
}

}  //  namespace test

}  //  namespace routing

}  //  namespace maidsafe
