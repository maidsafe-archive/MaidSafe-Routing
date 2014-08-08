/*  Copyright 2013 MaidSafe.net limited

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

#ifndef MAIDSAFE_ROUTING_PUBLIC_KEY_HOLDER_H_
#define MAIDSAFE_ROUTING_PUBLIC_KEY_HOLDER_H_

#include "maidsafe/routing/timer.h"


namespace maidsafe {

namespace routing {

class PublicKeyHolder {
 public:
  PublicKeyHolder(AsioService& asio_service);
  ~PublicKeyHolder();
  void Add(const NodeId& node_id, const asymm::PublicKey& public_key);
  boost::optional<asymm::PublicKey> Find(const NodeId& node_id) const;
  void Remove(const NodeId& node_id);
 private:
  mutable std::mutex mutex_;
  Timer<std::string> timer_;
  std::map<TaskId, std::pair<NodeId, asymm::PublicKey>> public_keys_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_PUBLIC_KEY_HOLDER_H_


