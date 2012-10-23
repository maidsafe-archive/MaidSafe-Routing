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

namespace fs = boost::filesystem;

namespace maidsafe {

namespace routing {

namespace test {

Commands::Commands(DemoNodePtr demo_node,
                   std::vector<maidsafe::Fob> all_fobs,
                   int identity_index) : demo_node_(demo_node),
                                         all_fobs_(all_fobs),
                                         identity_index_(identity_index),
                                         bootstrap_peer_ep_(),
                                         result_arrived_(false),
                                         finish_(false),
                                         wait_mutex_(),
                                         wait_cond_var_(),
                                         mark_results_arrived_() {
  mark_results_arrived_ = std::bind(&Commands::MarkResultArrived, this);
}

void Commands::Run() {
  PrintUsage();
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

void Commands::GetPeer(const Arguments &args) {
  if (args.size() != 1U) {
    std::cout << "Invalid number of arguments for GetPeer command.";
    return;
  }

  std::string peer(args[0]);
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
    peer_node_info.node_id = NodeId(all_fobs_[1].identity);
    peer_node_info.public_key = all_fobs_[1].keys.public_key;
  } else {
    peer_node_info.node_id = NodeId(all_fobs_[0].identity);
    peer_node_info.public_key = all_fobs_[0].keys.public_key;
  }
  peer_node_info.connection_id = peer_node_info.node_id;

  auto f1 = std::async(std::launch::async, [=] ()->int {
    return demo_node_->ZeroStateJoin(bootstrap_peer_ep_, peer_node_info);
  });
  EXPECT_EQ(kSuccess, f1.get());
}

void Commands::Join() {
  std::condition_variable cond_var;
  std::mutex mutex;

  std::weak_ptr<GenericNode> weak_node(demo_node_);
  demo_node_->functors_.network_status =
      [&cond_var, weak_node] (const int& result) {
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
          }
        }
      };
  std::vector<boost::asio::ip::udp::endpoint> bootstrap_endpoints;
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
  demo_node_->functors_.network_status = nullptr;
}

void Commands::PrintUsage() {
  std::cout << "\thelp Print options.\n";
  std::cout << "\tprt Print Routing Table.\n";
  std::cout << "\tpeer <endpoint> Set BootStrap peer endpoint.\n";
  std::cout << "\tzerostatejoin ZeroStateJoin.\n";
  std::cout << "\tjoin Normal Join.\n";
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
  } else if (cmd == "peer") {
    GetPeer(args);
  } else if (cmd == "zerostatejoin") {
    ZeroStateJoin();
  } else if (cmd == "join") {
    Join();
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


} // namespace test

} // namespace routing

} // namespace maidsafe