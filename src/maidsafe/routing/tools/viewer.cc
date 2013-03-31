/***************************************************************************************************
 *  Copyright 2012 MaidSafe.net limited                                                            *
 *                                                                                                 *
 *  The following source code is property of MaidSafe.net limited and is not meant for external    *
 *  use.  The use of this code is governed by the licence file licence.txt found in the root of    *
 *  this directory and also on www.maidsafe.net.                                                   *
 *                                                                                                 *
 *  You are not free to copy, amend or otherwise use this source code without the explicit         *
 *  written permission of the board of directors of MaidSafe.net.                                  *
 **************************************************************************************************/

#include <signal.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "boost/interprocess/ipc/message_queue.hpp"

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/common/log.h"
#include "maidsafe/common/on_scope_exit.h"

#include "maidsafe/routing/routing.pb.h"


namespace bi = boost::interprocess;

namespace {

const std::string kMessageQueueName("matrix_messages");
std::mutex g_mutex;
std::condition_variable g_cond_var;
bool g_ctrlc_pressed(false);

void SigHandler(int signum) {
  LOG(kInfo) << " Signal received: " << signum;
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_ctrlc_pressed = true;
  }
  g_cond_var.notify_one();
}

struct NodeInfo;

typedef std::set<NodeInfo, std::function<bool(const NodeInfo&, const NodeInfo&)>> NodeSet;
typedef std::set<NodeSet::iterator,
                 std::function<bool(NodeSet::iterator, NodeSet::iterator)>> MatrixSet;

struct NodeInfo {
  explicit NodeInfo(const maidsafe::NodeId& id_in)
      : id(id_in),
        matrix([id_in](NodeSet::iterator lhs, NodeSet::iterator rhs) {
                   return maidsafe::NodeId::CloserToTarget(lhs->id, rhs->id, id_in);
               }) {}
  NodeInfo(const NodeInfo& other) : id(other.id), matrix(other.matrix) {}
  NodeInfo(NodeInfo&& other) : id(std::move(other.id)), matrix(std::move(other.matrix)) {}
  NodeInfo& operator=(NodeInfo other) {
    using std::swap;
    swap(id, other.id);
    swap(matrix, other.matrix);
    return *this;
  }
  maidsafe::NodeId id;
  mutable MatrixSet matrix;
};

NodeSet g_nodes([](const NodeInfo& lhs, const NodeInfo& rhs) { return lhs.id < rhs.id; });

}  // unnamed namespace


namespace maidsafe {

void PrintDetails(const NodeInfo& node_info) {
  static int count(0);
  std::string printout(std::to_string(count++));
  printout += "\tReceived: Owner: " + DebugId(node_info.id) + "\n";
  for (const auto& node : node_info.matrix)
    printout += "\t\t" + DebugId(node->id) + "\n";
  LOG(kInfo) << printout << '\n';
}

MatrixSet::iterator FindInMatrix(const NodeInfo& node_info, const MatrixSet& matrix) {
  return std::find_if(std::begin(matrix), std::end(matrix),
                      [&node_info](NodeSet::iterator itr) { return itr->id == node_info.id; });
}

void EraseThisNodeFromReferreesMatrix(const NodeInfo& node_info, MatrixSet::value_type referee) {
  auto referred_entry(FindInMatrix(node_info, referee->matrix));
  assert(referred_entry != std::end(referee->matrix));
  referee->matrix.erase(referred_entry);
  if (referee->matrix.empty())
    g_nodes.erase(referee);
}

void EraseNode(const NodeInfo& node_info) {
  auto itr(g_nodes.find(node_info));
  if (itr == std::end(g_nodes))
    return;
  // Remove from all referrees (if referee's matrix becomes empty, erase referee from global set)
  // then erase this node from global set.
  for (auto referee : itr->matrix)
    EraseThisNodeFromReferreesMatrix(*itr, referee);
  g_nodes.erase(itr);
}

void InsertNode(const routing::protobuf::GetGroup& protobuf_matrix) {
  // Insert or find node
  auto itr(g_nodes.insert(NodeInfo(NodeId(protobuf_matrix.node_id()))).first);
  std::set<NodeId> sent_matrix;
  for (int i(0); i != protobuf_matrix.group_nodes_id_size(); ++i) {
    // If matrix entry doesn't already exist in this node's matrix, add entry or find entry in
    // global set, add this node to its matrix, then add it to this node's matrix.
    NodeInfo matrix_entry(NodeId(protobuf_matrix.group_nodes_id(i)));
    if (matrix_entry.id == itr->id)
      continue;
    sent_matrix.insert(matrix_entry.id);
    auto matrix_itr(FindInMatrix(matrix_entry, itr->matrix));
    if (matrix_itr != std::end(itr->matrix))
      continue;
    auto global_itr(g_nodes.insert(matrix_entry).first);
    global_itr->matrix.insert(itr);
    itr->matrix.insert(global_itr);
  }
  // Check all pre-existing matrix entries are still entries.  Any that aren't, remove this node
  // from its matrix
  auto referee_itr(std::begin(itr->matrix));
  while (referee_itr != std::end(itr->matrix)) {
    if (sent_matrix.find((*referee_itr)->id) == std::end(sent_matrix)) {
      EraseThisNodeFromReferreesMatrix(*itr, *referee_itr);
      referee_itr = itr->matrix.erase(referee_itr);
    } else {
      ++referee_itr;
    }
  }
  PrintDetails(*itr);
}

void UpdateNodeInfo(const std::string& serialised_matrix) {
  routing::protobuf::GetGroup protobuf_matrix;
  protobuf_matrix.ParseFromString(serialised_matrix);
  if (protobuf_matrix.group_nodes_id_size() == 0)
    EraseNode(NodeInfo(NodeId(protobuf_matrix.node_id())));
  else
    InsertNode(protobuf_matrix);
}

int Run(int argc, char **argv) {
#ifndef MAIDSAFE_WIN32
  signal(SIGHUP, SigHandler);
#endif
  signal(SIGINT, SigHandler);
  signal(SIGTERM, SigHandler);

  log::Logging::Instance().Initialise(argc, argv);
  try {
    bi::message_queue::remove(kMessageQueueName.c_str());
    on_scope_exit cleanup([]() { bi::message_queue::remove(kMessageQueueName.c_str()); });

    bi::message_queue matrix_messages(bi::create_only, kMessageQueueName.c_str(), 1000, 10000);
    LOG(kSuccess) << "Running...";
    unsigned int priority;
    bi::message_queue::size_type received_size;
    char input[10000];
    for (;;) {
      if (matrix_messages.try_receive(&input[0], 10000, received_size, priority)) {
        std::string received(&input[0], received_size);
        UpdateNodeInfo(received);
      }
      std::unique_lock<std::mutex> lock(g_mutex);
      if (g_cond_var.wait_for(lock, std::chrono::milliseconds(20), [] { return g_ctrlc_pressed; }))
        return 0;
    }
  }
  catch(bi::interprocess_exception &ex) {
    LOG(kError) << ex.what();
    return 1;
  }
}

}  // namespace maidsafe


int main(int argc, char **argv) {
  return maidsafe::Run(argc, argv);
}
