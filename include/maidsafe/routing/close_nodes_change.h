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

#ifndef MAIDSAFE_ROUTING_CLOSE_NODES_CHANGE_H_
#define MAIDSAFE_ROUTING_CLOSE_NODES_CHANGE_H_

#include <set>
#include <string>
#include <vector>

#include "maidsafe/common/config.h"
#include "maidsafe/common/crypto.h"
#include "maidsafe/common/node_id.h"
#include "maidsafe/common/utils.h"

namespace maidsafe {

namespace routing {

namespace test {
class CloseNodesChangeTest_BEH_CheckHolders_Test;
class SingleCloseNodesChangeTest_BEH_ChoosePmidNode_Test;
}

enum class GroupRangeStatus {
  kInRange,   // become in range (Parameter::group_size = 4)
  kInProximalRange,   // factor of holders' range (Parameter::group_size = 4)
  kOutwithRange   // become out of range (Parameter::group_size = 4)
};

struct CheckHoldersResult {
  routing::GroupRangeStatus proximity_status;
  NodeId new_holder;
};

class ConnectionsChange {
 protected:
  ConnectionsChange();
  ConnectionsChange(const ConnectionsChange& other);
  ConnectionsChange(ConnectionsChange&& other);
  ConnectionsChange& operator=(ConnectionsChange other);
  ConnectionsChange(NodeId this_node_id, const std::vector<NodeId>& old_close_nodes,
                   const std::vector<NodeId>& new_close_nodes);
 public:
  NodeId lost_node() const { return lost_node_; }
  NodeId new_node() const { return new_node_; }
  NodeId node_id() const { return node_id_; }

  std::string Print() const;

  friend void swap(ConnectionsChange& lhs, ConnectionsChange& rhs) MAIDSAFE_NOEXCEPT;

 protected:
  NodeId node_id_;
  NodeId lost_node_, new_node_;
};

class ClientNodesChange : public ConnectionsChange {
 public:
  ClientNodesChange();
  ClientNodesChange(const ClientNodesChange& other);
  ClientNodesChange(ClientNodesChange&& other);
  ClientNodesChange& operator=(ClientNodesChange other);
  ClientNodesChange(NodeId this_node_id, const std::vector<NodeId>& old_close_nodes,
                    const std::vector<NodeId>& new_close_nodes);
  std::string ReportConnection() const;
  std::string Print() const;
};

  class parent {
   public:
    parent() : node_id_(), lost_node_(), new_node_() {}
    parent(const NodeId& node_id, const NodeId& lost_node, const NodeId& new_node)
        : node_id_(node_id), lost_node_(lost_node), new_node_(new_node) {}
    parent(const NodeId& this_node_id,
                                         const std::vector<NodeId>& old_close_nodes,
                                         const std::vector<NodeId>& new_close_nodes)
    : node_id_(this_node_id),
    lost_node_([&, this]() -> NodeId {
//      assert(old_close_nodes.size() <= Parameters::closest_nodes_size);
//      assert(new_close_nodes.size() <= Parameters::closest_nodes_size);
      std::vector<NodeId> lost_nodes;
      for (const auto& old_node : old_close_nodes) {
        if (std::none_of(new_close_nodes.begin(), new_close_nodes.end(),
                         [&old_node](const NodeId& node_id){
                           return node_id == old_node;
                         }))
          lost_nodes.emplace_back(old_node);
      }
      assert(lost_nodes.size() <= 1);
      return (lost_nodes.empty()) ? NodeId() : lost_nodes.at(0);
    }()),
    new_node_([&, this]() -> NodeId {
      std::vector<NodeId> new_nodes;
      for (const auto& new_node : new_close_nodes) {
        if (std::none_of(old_close_nodes.begin(), old_close_nodes.end(),
                         [&new_node](const NodeId& node_id){
                           return node_id == new_node;
                         }))
          new_nodes.emplace_back(new_node);
      }
      assert(new_nodes.size() <= 1);
      return (new_nodes.empty()) ? NodeId() : new_nodes.at(0);
    }()) {}
    
    NodeId lost_node() const { return lost_node_; }
    NodeId new_node() const { return new_node_; }
    NodeId node_id_;
    NodeId lost_node_, new_node_;
  };
  
  class CloseNodesChange : public ConnectionsChange {
  public:
    CloseNodesChange();
    CloseNodesChange(const CloseNodesChange& other);
    CloseNodesChange(CloseNodesChange&& other);
    CloseNodesChange& operator=(CloseNodesChange other);
    CloseNodesChange(const NodeId& this_node_id, const std::vector<NodeId>& old_close_nodes,
                     const std::vector<NodeId>& new_close_nodes);
    CheckHoldersResult CheckHolders(const NodeId& target) const;
    bool CheckIsHolder(const NodeId& target, const NodeId& node_id) const;
    NodeId ChoosePmidNode(const std::set<NodeId>& online_pmids, const NodeId& target) const;
    std::vector<NodeId> new_close_nodes() const { return new_close_nodes_; }
    std::string Print() const;
    std::string ReportConnection() const;
    
    friend void swap(CloseNodesChange& lhs, CloseNodesChange& rhs) MAIDSAFE_NOEXCEPT;
    friend class RoutingTable;
    friend class test::CloseNodesChangeTest_BEH_CheckHolders_Test;
    friend class test::SingleCloseNodesChangeTest_BEH_ChoosePmidNode_Test;
    
  private:
    std::vector<NodeId> old_close_nodes_, new_close_nodes_;
    crypto::BigInt radius_;
  };


/*class CloseNodesChange : public ConnectionsChange {
 public:
  CloseNodesChange();
  CloseNodesChange(const CloseNodesChange& other);
  CloseNodesChange(CloseNodesChange&& other);
  CloseNodesChange& operator=(CloseNodesChange other);
  CloseNodesChange(NodeId this_node_id, const std::vector<NodeId>& old_close_nodes,
                   const std::vector<NodeId>& new_close_nodes);
  CheckHoldersResult CheckHolders(const NodeId& target) const;
  bool CheckIsHolder(const NodeId& target, const NodeId& node_id) const;
  NodeId ChoosePmidNode(const std::set<NodeId>& online_pmids, const NodeId& target) const;
  std::vector<NodeId> new_close_nodes() const { return new_close_nodes_; }
  std::string Print() const;
  std::string ReportConnection() const;

  friend void swap(CloseNodesChange& lhs, CloseNodesChange& rhs) MAIDSAFE_NOEXCEPT;

 private:
  std::vector<NodeId> old_close_nodes_, new_close_nodes_;
  crypto::BigInt radius_;
};*/

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_CLOSE_NODES_CHANGE_H_
