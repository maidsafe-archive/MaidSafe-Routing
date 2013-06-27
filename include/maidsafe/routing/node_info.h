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

#ifndef MAIDSAFE_ROUTING_NODE_INFO_H_
#define MAIDSAFE_ROUTING_NODE_INFO_H_

#include <cstdint>
#include <vector>

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/rsa.h"
#include "maidsafe/common/tagged_value.h"

#include "maidsafe/rudp/nat_type.h"


namespace maidsafe {

namespace routing {

struct NodeInfo {
  typedef TaggedValue<NonEmptyString, struct SerialisedNodeInfoTag> serialised_type;

  NodeInfo();

  NodeInfo(const NodeInfo& other);
  NodeInfo& operator=(const NodeInfo& other);
  NodeInfo(NodeInfo&& other);
  NodeInfo& operator=(NodeInfo&& other);
  explicit NodeInfo(const serialised_type& serialised_message);

  serialised_type Serialise() const;

  NodeId node_id;
  NodeId connection_id;  // Id of a node as far as rudp is concerned
  asymm::PublicKey public_key;
  int32_t rank;
  int32_t bucket;
  rudp::NatType nat_type;
  std::vector<int32_t> dimension_list;

  static const int32_t kInvalidBucket;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_NODE_INFO_H_
