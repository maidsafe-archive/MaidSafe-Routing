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

#include <vector>
#include <mutex>

#include "boost/asio/deadline_timer.hpp"
#include "boost/optional.hpp"

#include "maidsafe/common/rsa.h"
#include "maidsafe/common/asio_service.h"

#include "maidsafe/routing/parameters.h"


namespace maidsafe {

namespace routing {

class Network;

typedef std::shared_ptr<boost::asio::deadline_timer> TimerPointer;
typedef std::function<void(const boost::system::error_code& error)> Handler;

struct PublicKeyInfo {
  PublicKeyInfo(const NodeId& peer_in, const asymm::PublicKey public_key_in, TimerPointer timer_in)
      : peer(peer_in), public_key(public_key_in), timer(timer_in) {}
  NodeId peer;
  asymm::PublicKey public_key;
  TimerPointer timer;
};

class PublicKeyHolder {
 public:
  explicit PublicKeyHolder(AsioService& asio_service, Network& network);
  PublicKeyHolder(const PublicKeyHolder&) = delete;
  PublicKeyHolder& operator=(const PublicKeyHolder&) = delete;
  PublicKeyHolder(const PublicKeyHolder&&) = delete;
  PublicKeyHolder& operator=(const PublicKeyHolder&&) = delete;
  ~PublicKeyHolder();
  bool Add(const NodeId& peer, const asymm::PublicKey& public_key);
  boost::optional<asymm::PublicKey> Find(const NodeId& peer) const;
  void Remove(const NodeId& peer);

 private:
  mutable std::mutex mutex_;
  AsioService& io_service_;
  Network& network_;
  std::vector<PublicKeyInfo> elements_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_PUBLIC_KEY_HOLDER_H_

