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

#ifndef MAIDSAFE_ROUTING_TESTS_TEST_UTILS_H_
#define MAIDSAFE_ROUTING_TESTS_TEST_UTILS_H_

#include <cstdint>
#include <vector>

#include "boost/asio/ip/address.hpp"
#include "boost/asio/ip/udp.hpp"

#include "maidsafe/common/rsa.h"

#include "maidsafe/passport/types.h"

#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/routing_table.h"


namespace maidsafe {

namespace routing {

namespace test {

struct NodeInfoAndPrivateKey {
  NodeInfoAndPrivateKey()
      : node_info(),
        private_key() {}
  NodeInfo node_info;
  asymm::PrivateKey private_key;
};

NodeInfoAndPrivateKey MakeNodeInfoAndKeys();
NodeInfoAndPrivateKey MakeNodeInfoAndKeysWithPmid(passport::Pmid pmid);
NodeInfoAndPrivateKey MakeNodeInfoAndKeysWithMaid(passport::Maid maid);

passport::Maid MakeMaid();
passport::Pmid MakePmid();

// Fob GetFob(const NodeInfoAndPrivateKey& node);

NodeInfo MakeNode();

NodeId GenerateUniqueRandomId(const NodeId& holder, const uint16_t& pos);
NodeId GenerateUniqueRandomId(const uint16_t& pos);

int NetworkStatus(const bool& client, const int& status);

void SortFromTarget(const NodeId& target, std::vector<NodeInfo>& nodes);

void PartialSortFromTarget(const NodeId& target,
                           std::vector<NodeInfo>& nodes,
                           size_t num_to_sort);

void SortIdsFromTarget(const NodeId& target, std::vector<NodeId>& nodes);

void SortNodeInfosFromTarget(const NodeId& target, std::vector<NodeInfo>& nodes);

bool CompareListOfNodeInfos(const std::vector<NodeInfo>& lhs, const std::vector<NodeInfo>& rhs);

}  // namespace test

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_TESTS_TEST_UTILS_H_
