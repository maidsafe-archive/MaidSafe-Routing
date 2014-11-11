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

#include <memory>
#include <vector>

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/tests/main/test_utils.h"


namespace maidsafe {

namespace routing {

namespace test {

TEST(routing_table_test, FUNC_add_many_nodes_check_churn) {
  const auto network_size(500);
  auto nodes_to_remove(100);

  auto routing_tables(routing_table_network(network_size));
  std::vector<NodeId> node_ids;
  node_ids.reserve(network_size);

  // iterate and try to add each node to each other node
  for (auto& node : routing_tables) {
    node_ids.push_back(node->our_id());
    for (const auto& node_to_add : routing_tables) {
      node_info nodeinfo_to_add;
      nodeinfo_to_add.id = node_to_add->our_id();
      nodeinfo_to_add.public_key = node_to_add->our_public_key();
      node->add_node(nodeinfo_to_add);
    }
  }
  // now remove nodes
  std::vector<NodeId> drop_vec;
  drop_vec.reserve(nodes_to_remove);
  std::copy(std::begin(node_ids), std::begin(node_ids) + nodes_to_remove,
            std::back_inserter(drop_vec));
  routing_tables.erase(std::begin(routing_tables), std::begin(routing_tables) + nodes_to_remove);

  for (auto& node : routing_tables) {
    for (const auto& drop : drop_vec)
      node->drop_node(drop);
  }
  // remove ids too
  node_ids.erase(std::begin(node_ids), std::begin(node_ids) + nodes_to_remove);

  for (const auto& node : routing_tables) {
    size_t size = std::min(group_size, static_cast<size_t>(node->size()));
    auto id = node->our_id();
    // + 1 as node_ids includes our ID
    std::partial_sort(std::begin(node_ids), std::begin(node_ids) + size + 1, std::end(node_ids),
                      [id](const NodeId& lhs,
                           const NodeId& rhs) { return NodeId::CloserToTarget(lhs, rhs, id); });
    auto groups = node->our_close_group();
    EXPECT_EQ(groups.size(), size);
    for (size_t i = 0; i < size; ++i) {
      // + 1 as node_ids includes our ID
      EXPECT_EQ(groups.at(i).id, node_ids.at(i + 1)) << " node mismatch at " << i;
    }
  }
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
