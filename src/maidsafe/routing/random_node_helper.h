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

#ifndef MAIDSAFE_ROUTING_RANDOM_NODE_HELPER_H_
#define MAIDSAFE_ROUTING_RANDOM_NODE_HELPER_H_

#include <mutex>
#include <vector>

#include "maidsafe/common/node_id.h"


namespace maidsafe {

namespace routing {

class RandomNodeHelper {
 public:
  RandomNodeHelper() : node_ids_(), mutex_(), kMaxSize_(100) {}
  NodeId Get() const;
  void Add(const NodeId& node_id);
  void Remove(const NodeId& node_id);

 private:
  RandomNodeHelper(const RandomNodeHelper&);
  RandomNodeHelper(const RandomNodeHelper&&);
  RandomNodeHelper& operator=(const RandomNodeHelper&);

  std::vector<NodeId> node_ids_;
  mutable std::mutex mutex_;
  const size_t kMaxSize_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_RANDOM_NODE_HELPER_H_
