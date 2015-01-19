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

#include "maidsafe/routing/close_nodes_change.h"

#include <limits>
#include <sstream>
#include <utility>

#include "cereal/cereal.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/vector.hpp"

#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/utils.h"

namespace maidsafe {

namespace routing {

ConnectionsChange::ConnectionsChange()
    : node_id_(), lost_node_(), new_node_() {}

ConnectionsChange::ConnectionsChange(const ConnectionsChange& other)
    : node_id_(other.node_id_),
      lost_node_(other.lost_node_),
      new_node_(other.new_node_) {}

ConnectionsChange::ConnectionsChange(ConnectionsChange&& other)
    : node_id_(std::move(other.node_id_)),
      lost_node_(std::move(other.lost_node_)),
      new_node_(std::move(other.new_node_)) {}

ConnectionsChange& ConnectionsChange::operator=(ConnectionsChange other) {
  swap(*this, other);
  return *this;
}

ConnectionsChange::ConnectionsChange(NodeId this_node_id,
                                     const std::vector<NodeId>& old_close_nodes,
                                     const std::vector<NodeId>& new_close_nodes)
    : node_id_(std::move(this_node_id)),
      lost_node_([&, this]() -> NodeId {
        std::vector<NodeId> lost_nodes;
        std::set_difference(std::begin(old_close_nodes), std::end(old_close_nodes),
                            std::begin(new_close_nodes), std::end(new_close_nodes),
                            std::back_inserter(lost_nodes),
                            [this](const NodeId& lhs, const NodeId& rhs) {
          return NodeId::CloserToTarget(lhs, rhs, node_id_);
        });
        assert(lost_nodes.size() <= 1);
        return (lost_nodes.empty()) ? NodeId() : lost_nodes.at(0);
      }()),
      new_node_([&, this]() -> NodeId {
        std::vector<NodeId> new_nodes;
        std::set_difference(std::begin(new_close_nodes), std::end(new_close_nodes),
                            std::begin(old_close_nodes), std::end(old_close_nodes),
                            std::back_inserter(new_nodes),
                            [this](const NodeId& lhs, const NodeId& rhs) {
          return NodeId::CloserToTarget(lhs, rhs, node_id_);
        });
        assert(new_nodes.size() <= 1);
        return (new_nodes.empty()) ? NodeId() : new_nodes.at(0);
      }()) {}

std::string ConnectionsChange::Print() const {
  std::stringstream stream;
  stream << "\n\t\tentry in lost_node\t------\t" << lost_node_;
  stream << "\n\t\tentry in new_node\t------\t" << new_node_;
  return stream.str();
}

void swap(ConnectionsChange& lhs, ConnectionsChange& rhs) MAIDSAFE_NOEXCEPT {
  using std::swap;
  swap(lhs.node_id_, rhs.node_id_);
  swap(lhs.lost_node_, rhs.lost_node_);
  swap(lhs.new_node_, rhs.new_node_);
}

ClientNodesChange::ClientNodesChange() : ConnectionsChange() {}
ClientNodesChange::ClientNodesChange(const ClientNodesChange& other)
    : ConnectionsChange(other) {}

ClientNodesChange::ClientNodesChange(ClientNodesChange&& other)
    : ConnectionsChange(std::move(other)) {}

ClientNodesChange::ClientNodesChange& operator=(ClientNodesChange other) {
  std::swap(*this, other);
  return *this;
}

ClientNodesChange::ClientNodesChange(NodeId this_node_id,
                                     const std::vector<NodeId>& old_close_nodes,
                                     const std::vector<NodeId>& new_close_nodes)
    : ConnectionsChange(this_node_id, old_close_nodes, new_close_nodes) {}

std::string ClientNodesChange::ReportConnection() const {
  return ConnectionsChange::Print();
}

std::string ClientNodesChange::Print() const {
  return ConnectionsChange::Print();
}

std::string CloseNodesChange::ReportConnection() const {
  std::stringstream stringstream;
  {
    cereal::JSONOutputArchive archive{stringstream};
    if (lost_node().IsValid())
      archive(cereal::make_nvp("vaultRemoved",
                               lost_node().ToStringEncoded(NodeId::EncodingType::kHex)));
    if (new_node().IsValid())
      archive(cereal::make_nvp("vaultAdded",
                               new_node().ToStringEncoded(NodeId::EncodingType::kHex)));

    size_t closest_size_adjust(std::min(new_close_nodes_.size(),
                                        static_cast<size_t>(Parameters::group_size + 1U)));

    if (closest_size_adjust != 0) {
      std::vector<NodeId> closest_nodes(closest_size_adjust);
      std::partial_sort_copy(std::begin(new_close_nodes_), std::end(new_close_nodes_),
                             std::begin(closest_nodes), std::end(closest_nodes),
                             [&](const NodeId& lhs, const NodeId& rhs) {
                               return NodeId::CloserToTarget(lhs, rhs, this->node_id());
                             });
      if (node_id() == closest_nodes.front())
        closest_nodes.erase(std::begin(closest_nodes));
      else if (closest_nodes.size() > Parameters::group_size)
        closest_nodes.pop_back();

      if (closest_nodes.size() != 0) {
        std::vector<std::string> closest_ids;
        for (auto& close_node : closest_nodes)
          closest_ids.emplace_back(close_node.ToStringEncoded(NodeId::EncodingType::kHex));
        archive(cereal::make_nvp("closeGroupVaults", closest_ids));
      }
    }
  }
  return stringstream.str();
}

void swap(CloseNodesChange& lhs, CloseNodesChange& rhs) MAIDSAFE_NOEXCEPT {
  using std::swap;
  swap(dynamic_cast<ConnectionsChange&>(lhs), dynamic_cast<ConnectionsChange&>(rhs));
  swap(lhs.old_close_nodes_, rhs.old_close_nodes_);
  swap(lhs.new_close_nodes_, rhs.new_close_nodes_);
  swap(lhs.radius_, rhs.radius_);
}

std::string CloseNodesChange::Print() const {
  std::stringstream stream;
  for (const auto& node_id : old_close_nodes_)
    stream << "\n\t\tentry in old_close_nodes\t------\t" << node_id;

  for (const auto& node_id : new_close_nodes_)
    stream << "\n\t\tentry in new_close_nodes\t------\t" << node_id;

  stream << ConnectionsChange::Print();

  return stream.str();
}


}  // namespace routing

}  // namespace maidsafe
