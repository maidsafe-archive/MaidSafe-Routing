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
#include "maidsafe/tools/shared_response.h"

namespace fs = boost::filesystem;

namespace maidsafe {

namespace routing {

namespace test {

Commands::Commands(DemoNodePtr demo_node,
                   std::vector<maidsafe::passport::Pmid> all_pmids,
                   int identity_index) : demo_node_(demo_node),
                                         all_pmids_(all_pmids),
                                         all_ids_(),
                                         identity_index_(identity_index),
                                         bootstrap_peer_ep_(),
                                         data_size_(256 * 1024),
                                         result_arrived_(false),
                                         finish_(false),
                                         wait_mutex_(),
                                         wait_cond_var_(),
                                         mark_results_arrived_() {
  // CalculateClosests will only use all_ids_ to calculate expected respondents
  // here it is assumed that the first half of fobs will be used as vault
  // and the latter half part will be used as client, which shall not respond msg
  // i.e. shall not be put into all_ids_
  for (size_t i(0); i < (all_pmids_.size() / 2); ++i)
    all_ids_.push_back(NodeId(all_pmids_[i].name().data));

  demo_node->functors_.request_public_key = [this] (const NodeId& node_id,
                                                    GivePublicKeyFunctor give_public_key) {
      this->Validate(node_id, give_public_key);
  };
  demo_node->functors_.message_received = [this] (const std::string &wrapped_message,
                                                  const NodeId &/*group_claim*/,
                                                  bool /* cache */,
                                                  const ReplyFunctor &reply_functor) {
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

  auto iter(all_pmids_.begin());
  bool find(false);
  while ((iter != all_pmids_.end()) && !find) {
    if (iter->name().data.string() == node_id.string())
      find = true;
    else
      ++iter;
  }
  if (iter != all_pmids_.end())
    give_public_key((*iter).public_key());
}

void Commands::Run() {
  PrintUsage();

  if ((!demo_node_->joined()) && (identity_index_ >= 2)) {// &&
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
      boost::mutex::scoped_lock lock(wait_mutex_);
      ProcessCommand(cmdline);
      wait_cond_var_.wait(lock, boost::bind(&Commands::ResultArrived, this));
      result_arrived_ = false;
    }
  }
}

void Commands::PrintRoutingTable() {
  demo_node_->PrintRoutingTable();
}

void Commands::GetPeer(const std::string &peer) {
  size_t delim = peer.rfind(':');
  try {
    bootstrap_peer_ep_.port(boost::lexical_cast<uint16_t>(peer.substr(delim + 1)));
    bootstrap_peer_ep_.address(boost::asio::ip::address::from_string(peer.substr(0, delim)));
    std::cout << "Going to bootstrap from endpoint " << bootstrap_peer_ep_ << std::endl;
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
    peer_node_info.node_id = NodeId(all_pmids_[1].name().data);
    peer_node_info.public_key = all_pmids_[1].public_key();
  } else {
    peer_node_info.node_id = NodeId(all_pmids_[0].name().data);
    peer_node_info.public_key = all_pmids_[0].public_key();
  }
  peer_node_info.connection_id = peer_node_info.node_id;

  auto f1 = std::async(std::launch::async, [=] ()->int {
    return demo_node_->ZeroStateJoin(bootstrap_peer_ep_, peer_node_info);
  });
  EXPECT_EQ(kSuccess, f1.get());
}

void Commands::SendMsgs(const int& id_index, const DestinationType& destination_type,
                        bool is_routing_req, int num_msg) {
  std::string data, data_to_send;
  uint32_t message_id(0);

  //  Check message type
  if (is_routing_req)
    data = "request_routing_table";
  else
    data_to_send = data = RandomAlphaNumericString(data_size_);

  bool infinite(false);
  if (num_msg == -1)
    infinite = true;

  //   Send messages
  for (int index = 0; index < num_msg || infinite; ++index) {
    std::vector<NodeId> closest_nodes;
    uint16_t expect_respondent = MakeMessage(id_index, destination_type, closest_nodes);
    if (expect_respondent == 0)
      return;
    std::shared_ptr<SharedResponse> resp_collector_ptr;
    {
      resp_collector_ptr = std::make_shared<SharedResponse>(closest_nodes, expect_respondent);
      auto callable = [=](std::string response) {
        if (!response.empty()) {
          resp_collector_ptr->CollectResponse(response);
          if (resp_collector_ptr->expected_responses == 1)
            resp_collector_ptr->PrintRoutingTable(response);
      }
      };
      //  Send the msg
      data = ">:<" + boost::lexical_cast<std::string>(++message_id) + "<:>" + data;
      /*demo_node_->Send(dest_id, NodeId(), data, callable,
                       boost::posix_time::seconds(12), destination_type, false);*/
    }
    data = data_to_send;
  }
}



uint16_t Commands::MakeMessage(const int& id_index, const DestinationType& destination_type,
                               std::vector<NodeId> &closest_nodes) {
  int identity_index;
  if (id_index >= 0)
      identity_index = id_index;
    else
      identity_index = RandomUint32() % (all_pmids_.size() / 2);
  if ((identity_index >= 0) && (static_cast<uint32_t>(identity_index) >= all_pmids_.size())) {
    std::cout << "ERROR : destination index out of range" << std::endl;
    return 0;
  }
  NodeId dest_id(RandomString(64));
  if (identity_index >= 0)
    dest_id = NodeId(all_pmids_[identity_index].name().data);
  std::cout << "Sending a msg from : " << maidsafe::HexSubstr(demo_node_->node_id().string())
            << " to " << (destination_type != DestinationType::kGroup ? ": " : "group : ")
            << maidsafe::HexSubstr(dest_id.string())
            << " , expect receive response from :" << std::endl;
  uint16_t expected_respodents(destination_type != DestinationType::kGroup ? 1 : 4);
  std::vector<NodeId> closests;
  NodeId farthest_closests(CalculateClosests(dest_id, closests, expected_respodents));
  for (auto &node_id : closests)
  std::cout << "\t" << maidsafe::HexSubstr(node_id.string()) << std::endl;
  closest_nodes = closests;
  return expected_respodents;
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
            std::cout << "Network Status Changed" << std::endl;
            this->PrintRoutingTable();
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
    SendMsgs(boost::lexical_cast<int>(args[0]), DestinationType::kDirect, true, 1);
  } else if (cmd == "peer") {
    GetPeer(args[0]);
  } else if (cmd == "zerostatejoin") {
    ZeroStateJoin();
  } else if (cmd == "join") {
    Join();
  } else if (cmd == "senddirect") {
    if (args.size() == 1) {
      SendMsgs(boost::lexical_cast<int>(args[0]), DestinationType::kDirect, false, 1);
    } else if (args.size() == 2) {
      int count(boost::lexical_cast<int>(args[1]));
      bool infinite(count < 0);
      if (infinite)
        std::cout << " Running infinite messaging test. press Ctrl + C to terminate the program"
                  << std::endl;
      SendMsgs(boost::lexical_cast<int>(args[0]), DestinationType::kDirect, false, -1);
    }
  } else if (cmd == "sendgroup") {
    if (args.empty())
      SendMsgs(-1, DestinationType::kGroup, false, 1);
    else
      SendMsgs(boost::lexical_cast<int>(args[0]), DestinationType::kGroup, false, 1);
  } else if (cmd == "sendgroupmultiple") {
    if (args.size() == 1) {
      SendMsgs(-1, DestinationType::kGroup, false, boost::lexical_cast<int>(args[0]));
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
    boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
    SendMsgs(-1, DestinationType::kDirect, false, -1);
    SendMsgs(-1, DestinationType::kDirect, false, -1);
    std::cout << "Sent " << num_msg << " messages to randomly picked-up targets. Finished in :"
              << boost::posix_time::microsec_clock::universal_time() - now << std::endl;
  } else if (cmd == "datasize") {
    data_size_ = boost::lexical_cast<int>(args[0]);
  } else if (cmd == "datarate") {
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
  boost::mutex::scoped_lock lock(wait_mutex_);
  result_arrived_ = true;
  wait_cond_var_.notify_one();
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


}  //  namespace test

}  //  namespace routing

}  //  namespace maidsafe
