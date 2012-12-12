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

#include "maidsafe/tools/commands.h"

#include <iostream> // NOLINT
#include <sstream>

#include "boost/format.hpp"
#include "boost/filesystem.hpp"
#ifdef __MSVC__
# pragma warning(push)
# pragma warning(disable: 4127)
#endif
#include "boost/tokenizer.hpp"
#ifdef __MSVC__
# pragma warning(pop)
#endif
#include "boost/lexical_cast.hpp"
#include "maidsafe/common/crypto.h"
#include "maidsafe/common/utils.h"

namespace fs = boost::filesystem;

namespace maidsafe {

namespace routing {

namespace test {

Commands::Commands(std::string fobs_path_str,
                   bool client_only_node,
                   int identity_index) : demo_node_(),
                                         all_fobs_(),
                                         all_ids_(),
                                         identity_index_(identity_index),
                                         node_info_(),
                                         bootstrap_peer_ep_(),
                                         data_size_(256 * 1024),
                                         result_arrived_(false),
                                         finish_(false),
                                         boot_strap_(),
                                         peer_routing_table_(),
                                         wait_mutex_(new boost::mutex()),
                                         wait_cond_var_(new boost::condition_variable()),
                                         mark_results_arrived_() {
  boost::system::error_code error_code;
  maidsafe::Fob this_fob;
  fs::path fobs_path;
  if (fobs_path_str.empty())
    fobs_path = fs::path(fs::temp_directory_path(error_code) / "fob_list.dat");
  else
    fobs_path = fs::path(fobs_path_str);
  if (fs::exists(fobs_path, error_code)) {
    all_fobs_ = maidsafe::routing::ReadFobList(fobs_path);
#ifndef ROUTING_PYTHON_API
    std::cout << "Loaded " << all_fobs_.size() << " fobs." << std::endl;
#endif
    if (static_cast<uint32_t>(identity_index) >= all_fobs_.size() || identity_index < 0) {
      std::cout << "ERROR : index exceeds fob pool -- pool has "
                << all_fobs_.size() << " fobs, while identity_index is "
                << identity_index << std::endl;
// shall throw here
      return;
    } else {
      this_fob = all_fobs_[identity_index];
#ifndef ROUTING_PYTHON_API
      std::cout << "Using identity #" << identity_index << " from keys file"
                << " , value is : " << maidsafe::HexSubstr(this_fob.identity) << std::endl;
#endif
    }
  }
  // Ensure correct index range is being used
  if (client_only_node) {
    if (identity_index < static_cast<int>(all_fobs_.size() / 2)) {
      std::cout << "ERROR : Incorrect identity_index used for a client, must between "
                << all_fobs_.size() / 2 << " and " << all_fobs_.size() - 1 << std::endl;
// shall throw here
      return;
    }
  } else {
    if (identity_index >= static_cast<int>(all_fobs_.size() / 2)) {
      std::cout << "ERROR : Incorrect identity_index used for a vault, must between 0 and "
                << all_fobs_.size() / 2 - 1 << std::endl;
// shall throw here
      return;
    }
  }

  // CalculateClosests will only use all_ids_ to calculate expected respondents
  // here it is assumed that the first half of fobs will be used as vault
  // and the latter half part will be used as client, which shall not respond msg
  // i.e. shall not be put into all_ids_
  for (size_t i(0); i < (all_fobs_.size() / 2); ++i)
    all_ids_.push_back(NodeId(all_fobs_[i].identity));

  mark_results_arrived_ = std::bind(&Commands::MarkResultArrived, this);

  InitDemoNode(client_only_node, this_fob);
}

void Commands::InitDemoNode(bool client_only_node, const Fob& this_fob) {
  maidsafe::routing::test::NodeInfoAndPrivateKey node_info(
      maidsafe::routing::test::MakeNodeInfoAndKeysWithFob(this_fob));
  demo_node_.reset(new maidsafe::routing::test::GenericNode(client_only_node, node_info));
  if (identity_index_ < 2) {
#ifndef ROUTING_PYTHON_API
    std::cout << "------ Current BootStrap node endpoint info : "
              << demo_node_->endpoint() << " ------ " << std::endl;
#endif
  }
  std::ostringstream convert;
  convert <<  demo_node_->endpoint();
  node_info_ = convert.str();

  demo_node_->functors_.request_public_key = [this] (const NodeId& node_id,
                                                     GivePublicKeyFunctor give_public_key) {
      this->Validate(node_id, give_public_key);
  };
  demo_node_->functors_.message_received = [this] (const std::string &wrapped_message,
                                                   const NodeId &/*group_claim*/,
                                                   bool /* cache */,
                                                   const ReplyFunctor &reply_functor) {
    std::string reply_msg(wrapped_message + "+++" + demo_node_->fob().identity.string());
    if (std::string::npos != wrapped_message.find("request_routing_table"))
      reply_msg = reply_msg + "---" + demo_node_->SerializeRoutingTable();
    reply_functor(reply_msg);
  };
}

void Commands::Validate(const NodeId& node_id, GivePublicKeyFunctor give_public_key) {
  if (node_id == NodeId())
    return;

  auto iter(all_fobs_.begin());
  bool find(false);
  while ((iter != all_fobs_.end()) && !find) {
    if (iter->identity.string() == node_id.string())
      find = true;
    else
      ++iter;
  }
  if (iter != all_fobs_.end())
    give_public_key((*iter).keys.public_key);
}

void Commands::Run() {
  PrintUsage();

  if ((!demo_node_->joined()) && (identity_index_ >= 2)/* &&
      (bootstrap_peer_ep_ != boost::asio::ip::udp::endpoint())*/) {
    // All parameters have been setup via cmdline directly, join the node immediately
    std::cout << "Joining the node ......" << std::endl;
    Join();
  }

  while (!finish_) {
    std::cout << std::endl << std::endl << "Enter command > ";
    std::string cmdline;
    std::getline(std::cin, cmdline);
    {
      boost::mutex::scoped_lock lock(*wait_mutex_);
      ProcessCommand(cmdline);
      wait_cond_var_->wait(lock, boost::bind(&Commands::ResultArrived, this));
      result_arrived_ = false;
    }
  }
}

void Commands::PrintRoutingTable() {
  demo_node_->PrintRoutingTable();
}

void Commands::SetPeer(std::string peer) {
  size_t delim = peer.rfind(':');
  try {
    bootstrap_peer_ep_.port(boost::lexical_cast<uint16_t>(peer.substr(delim + 1)));
    bootstrap_peer_ep_.address(boost::asio::ip::address::from_string(peer.substr(0, delim)));
#ifndef ROUTING_PYTHON_API
    std::cout << "Going to bootstrap from endpoint " << bootstrap_peer_ep_ << std::endl;
#endif
  }
  catch(...) {
    std::cout << "Could not parse IPv4 peer endpoint from " << peer << std::endl;
  }
}

void Commands::ZeroStateJoin() {
  if (identity_index_ > 1) {
    std::cout << "can't exec ZeroStateJoin as a non-bootstrap node" << std::endl;
    return;
  }
  NodeInfo peer_node_info;
  if (identity_index_ == 0) {
    peer_node_info.node_id = NodeId(all_fobs_[1].identity);
    peer_node_info.public_key = all_fobs_[1].keys.public_key;
  } else {
    peer_node_info.node_id = NodeId(all_fobs_[0].identity);
    peer_node_info.public_key = all_fobs_[0].keys.public_key;
  }
  peer_node_info.connection_id = peer_node_info.node_id;
  boot_strap_.reset(new std::future<int>(std::async(std::launch::async, [=] ()->int {
    return demo_node_->ZeroStateJoin(bootstrap_peer_ep_, peer_node_info);})));
}

bool Commands::SendAMessage(int identity_index, int destination_type, int data_size) {
  std::string data(RandomAlphaNumericString(data_size));
  switch (destination_type) {
    case 0 :
      return SendAMsg(identity_index, DestinationType::kDirect, data);
      break;
    case 1 :
      return SendAMsg(identity_index, DestinationType::kGroup, data);
      break;
    case 2 :
      return SendAMsg(identity_index, DestinationType::kClosest, data);
      break;
    default :
      return false;
  }
  return true;
}

int Commands::RoutingTableSize() {
  return demo_node_->RoutingTable().size();
}

bool Commands::ExistInRoutingTable(int peer_index) {
  NodeId peer_id(all_fobs_[peer_index].identity.string());
  return demo_node_->RoutingTableHasNode(peer_id);
}

bool Commands::SendAMsg(const int& identity_index, const DestinationType& destination_type,
                        std::string& data) {
  if ((identity_index >= 0) && (static_cast<uint32_t>(identity_index) >= all_fobs_.size())) {
    std::cout << "ERROR : destination index out of range" << std::endl;
    return false;
  }
  NodeId dest_id(RandomString(64));
  if (identity_index >= 0)
    dest_id = NodeId(all_fobs_[identity_index].identity);
#ifndef ROUTING_PYTHON_API
  std::cout << "Sending a msg from : " << maidsafe::HexSubstr(demo_node_->fob().identity)
            << " to " << (destination_type != DestinationType::kGroup ? ": " : "group : ")
            << maidsafe::HexSubstr(dest_id.string())
            << " , expect receive response from :" << std::endl;
#endif
  uint16_t expected_respodents(destination_type != DestinationType::kGroup ? 1 : 4);
  std::vector<NodeId> closests;
  NodeId farthest_closests(CalculateClosests(dest_id, closests, expected_respodents));
#ifndef ROUTING_PYTHON_API
  for (auto &node_id : closests)
    std::cout << "\t" << maidsafe::HexSubstr(node_id.string()) << std::endl;
#endif

  std::set<NodeId> responsed_nodes;
  std::string received_response_msg;
  std::mutex mutex;
  std::condition_variable cond_var;
  size_t messages_count(0), message_id(0), expected_messages(1);
  auto callable = [&](const std::vector<std::string> &message) {
    if (message.empty())
      return;
    std::lock_guard<std::mutex> lock(mutex);
    messages_count++;
    for (auto &msg : message) {
      std::string response_id(msg.substr(msg.find("+++") + 3, 64));
      responsed_nodes.insert(NodeId(response_id));
      received_response_msg = msg;
    }
    if (responsed_nodes.size() == expected_respodents)
      cond_var.notify_one();
  };

  {
    std::lock_guard<std::mutex> lock(mutex);
    data = ">:<" + boost::lexical_cast<std::string>(++message_id) + "<:>" + data;
  }
#ifndef ROUTING_PYTHON_API
  boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
#endif
  demo_node_->Send(dest_id, NodeId(), data, callable,
                   boost::posix_time::seconds(12), destination_type, false);

  std::unique_lock<std::mutex> lock(mutex);
  bool result = cond_var.wait_for(lock, std::chrono::seconds(20),
      [&]()->bool { return messages_count == expected_messages; });
#ifndef ROUTING_PYTHON_API
  std::cout << "Response received in "
            << boost::posix_time::microsec_clock::universal_time() - now << std::endl;
  EXPECT_TRUE(result) << "Failure in sending message";

  std::cout << "Received response from following nodes :" << std::endl;
#endif
  for(auto &responsed_node : responsed_nodes) {
#ifndef ROUTING_PYTHON_API
    std::cout << "\t" << maidsafe::HexSubstr(responsed_node.string()) << std::endl;
#endif
    if (destination_type == DestinationType::kGroup)
      EXPECT_TRUE(std::find(closests.begin(), closests.end(), responsed_node) != closests.end());
    else
      EXPECT_EQ(responsed_node, dest_id);
  }

  if (std::string::npos != received_response_msg.find("request_routing_table")) {
    std::string response_node_list_msg(
        received_response_msg.substr(received_response_msg.find("---") + 3,
            received_response_msg.size() - (received_response_msg.find("---") + 3)));
    peer_routing_table_.clear();
    peer_routing_table_ = maidsafe::routing::DeserializeNodeIdList(response_node_list_msg);
#ifndef ROUTING_PYTHON_API
    std::cout << "Received routing table from peer is :" << std::endl;
    for (auto &node_id : peer_routing_table_)
      std::cout << "\t" << maidsafe::HexSubstr(node_id.string()) << std::endl;
#endif
  }
  return result;
}

void Commands::Join() {
  if (demo_node_->joined()) {
    std::cout << "Current node already joined" << std::endl;
    return;
  }
  std::condition_variable cond_var;
  std::mutex mutex;

  std::weak_ptr<GenericNode> weak_node(demo_node_);
  demo_node_->functors_.network_status =
      [this, &cond_var, weak_node] (const int& result) {
        if (std::shared_ptr<GenericNode> node = weak_node.lock()) {
          if (!node->anonymous()) {
            ASSERT_GE(result, kSuccess);
          } else  {
            if (!node->joined()) {
              ASSERT_EQ(result, kSuccess);
            } else if (node->joined()) {
              ASSERT_EQ(result, kAnonymousSessionEnded);
            }
          }
          if ((result == node->expected() && !node->joined()) || node->anonymous()) {
            node->set_joined(true);
            cond_var.notify_one();
          } else {
#ifndef ROUTING_PYTHON_API
            std::cout << "Network Status Changed" << std::endl;
            this->PrintRoutingTable();
#endif
          }
        }
      };
  std::vector<boost::asio::ip::udp::endpoint> bootstrap_endpoints;
  if (bootstrap_peer_ep_ != boost::asio::ip::udp::endpoint())
    bootstrap_endpoints.push_back(bootstrap_peer_ep_);
  demo_node_->Join(bootstrap_endpoints);

  if (!demo_node_->joined()) {
    std::unique_lock<std::mutex> lock(mutex);
    auto result = cond_var.wait_for(lock, std::chrono::seconds(20));
    EXPECT_EQ(result, std::cv_status::no_timeout);
    Sleep(boost::posix_time::millisec(600));
  }
#ifndef ROUTING_PYTHON_API
  std::cout << "Current Node joined, following is the routing table :" << std::endl;
  PrintRoutingTable();
#endif
}

void Commands::PrintUsage() {
  std::cout << "\thelp Print options.\n";
  std::cout << "\tpeer <endpoint> Set BootStrap peer endpoint.\n";
  std::cout << "\tzerostatejoin ZeroStateJoin.\n";
  std::cout << "\tjoin Normal Join.\n";
  std::cout << "\tprt Print Local Routing Table.\n";
  std::cout << "\trrt <dest_index> Request Routing Table from peer node with the specified identity-index.\n";
  std::cout << "\tsenddirect <dest_index> <num_msg> Send a msg to a node with specified identity-index."
            << " -1 for infinite (Default 1)\n";
  std::cout << "\tsendgroup <dest_index> Send a msg to group (default is Random GroupId,"
            << " dest_index for using existing identity as a group_id)\n";
  std::cout << "\tsendmultiple <num_msg> Send num of msg to randomly picked-up destination."
            << " -1 for infinite (Default 10)\n";
  std::cout << "\tsendgroupmultiple <num_msg> Send num of group msg to randomly "
            << " picked-up destination. -1 for infinite\n";
  std::cout << "\tdatasize <data_size> Set the data_size for the message.\n";
  std::cout << "\nattype Print the NatType of this node.\n";
  std::cout << "\texit Exit application.\n";
}

void Commands::ProcessCommand(const std::string &cmdline) {
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
  catch(const std::exception &e) {
    LOG(kError) << "Error processing command: " << e.what();
  }

  if (cmd == "help") {
    PrintUsage();
  } else if (cmd == "prt") {
    PrintRoutingTable();
  } else if (cmd == "rrt") {
    std::string data("request_routing_table");
    SendAMsg(boost::lexical_cast<int>(args[0]), DestinationType::kDirect, data);
  } else if (cmd == "peer") {
    SetPeer(args[0]);
  } else if (cmd == "zerostatejoin") {
    ZeroStateJoin();
  } else if (cmd == "join") {
    Join();
  } else if (cmd == "senddirect") {
    std::string data(RandomAlphaNumericString(data_size_));
    if (args.size() == 1) {
      SendAMsg(boost::lexical_cast<int>(args[0]), DestinationType::kDirect, data);
    } else if (args.size() == 2) {
      int count(boost::lexical_cast<int>(args[1]));
      bool infinite(count < 0);
      if (infinite)
        std::cout << " Running infinite messaging test. press Ctrl + C to terminate the program"
                  << std::endl;
      for (auto i(0); (i < count) || infinite; ++i) {
        std::cout << "sending " << i << "th message" << std::endl;
        SendAMsg(boost::lexical_cast<int>(args[0]), DestinationType::kDirect, data);
      }
    }
  } else if (cmd == "sendgroup") {
    std::string data(RandomAlphaNumericString(data_size_));
    if (args.empty())
      SendAMsg(-1, DestinationType::kGroup, data);
    else
      SendAMsg(boost::lexical_cast<int>(args[0]), DestinationType::kGroup, data);
  } else if (cmd == "sendgroupmultiple") {
    if (args.size() == 1) {
      int index(0);
      while(index++ != boost::lexical_cast<int>(args[0])) {
        std::cout << "sending " << index << "th group message" << std::endl;
        std::string data(RandomAlphaNumericString(data_size_));
        SendAMsg(-1, DestinationType::kGroup, data);
      }
    }
  } else if (cmd == "sendmultiple") {
    int num_msg(10);
    if (!args.empty())
      num_msg = boost::lexical_cast<int>(args[0]);
    bool infinite(false);
    if (num_msg == -1) {
      infinite = true;
      std::cout << " Running infinite messaging test. press Ctrl + C to terminate the program"
                << std::endl;
    }
    std::string data(RandomAlphaNumericString(data_size_));
    boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
    while (infinite)
      SendAMsg(RandomUint32() % (all_fobs_.size() / 2), DestinationType::kDirect, data);

    for (int i(0); i < num_msg; ++i) {
      SendAMsg(RandomUint32() % (all_fobs_.size() / 2), DestinationType::kDirect, data);
    }
    std::cout << "Sent " << num_msg << " messages to randomly picked-up targets. Finished in :"
              << boost::posix_time::microsec_clock::universal_time() - now << std::endl;
  } else if (cmd == "datasize") {
    data_size_ = boost::lexical_cast<int>(args[0]);
  } else if (cmd == "nattype") {
    std::cout << "NatType for this node is : " << demo_node_->nat_type() << std::endl;
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
  boost::mutex::scoped_lock lock(*wait_mutex_);
  result_arrived_ = true;

  wait_cond_var_->notify_one();
}

NodeId Commands::CalculateClosests(const NodeId& target_id,
                                   std::vector<NodeId>& closests,
                                   uint16_t num_of_closests) {
  if (all_ids_.size() <= num_of_closests) {
    closests = all_ids_;
    return closests[closests.size() - 1];
  }
  std::sort(all_ids_.begin(), all_ids_.end(),
            [&](const NodeId& lhs, const NodeId& rhs) {
              return NodeId::CloserToTarget(lhs, rhs, target_id);
            });
  closests = std::vector<NodeId>(all_ids_.begin() +
                                     boost::lexical_cast<bool>(all_ids_[0] == target_id),
                                 all_ids_.begin() + num_of_closests +
                                     boost::lexical_cast<bool>(all_ids_[0] == target_id));
  return closests[closests.size() - 1];
}


} // namespace test

} // namespace routing

} // namespace maidsafe
