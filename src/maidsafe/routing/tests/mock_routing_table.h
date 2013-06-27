/* Copyright 2012 MaidSafe.net limited

This MaidSafe Software is licensed under the MaidSafe.net Commercial License, version 1.0 or later,
and The General Public License (GPL), version 3. By contributing code to this project You agree to
the terms laid out in the MaidSafe Contributor Agreement, version 1.0, found in the root directory
of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also available at:

http://www.novinet.com/license

Unless required by applicable law or agreed to in writing, software distributed under the License is
distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
implied. See the License for the specific language governing permissions and limitations under the
License.
*/

#ifndef MAIDSAFE_ROUTING_TESTS_MOCK_ROUTING_TABLE_H_
#define MAIDSAFE_ROUTING_TESTS_MOCK_ROUTING_TABLE_H_

#include <string>

#include "gmock/gmock.h"

#include "maidsafe/routing/routing_table.h"


namespace maidsafe {

namespace routing {

namespace test {

class MockRoutingTable : public RoutingTable {
 public:
  MockRoutingTable(bool client_mode, const NodeId& node_id, const asymm::Keys& keys,
                   NetworkStatistics& network_statistics);
  virtual ~MockRoutingTable();

  MOCK_METHOD2(IsNodeIdInGroupRange, bool(const NodeId& node_id, bool& is_group_leader));

 private:
  MockRoutingTable &operator=(const MockRoutingTable&);
  MockRoutingTable(const MockRoutingTable&);
};

}  // namespace test

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_TESTS_MOCK_ROUTING_TABLE_H_

