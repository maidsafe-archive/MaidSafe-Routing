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

#include "maidsafe/routing/public_key_holder.h"

#include "maidsafe/routing/network.h"

namespace maidsafe {

namespace routing {

PublicKeyHolder::PublicKeyHolder(AsioService& io_service, Network &network)
    : mutex_(), io_service_(io_service), network_(network), elements_() {}

PublicKeyHolder::~PublicKeyHolder() {
  std::for_each(std::begin(elements_), std::end(elements_),
                [](const PublicKeyInfo& info) {
                  info.timer->cancel();
                });
  while (!elements_.empty()) {}
}

bool PublicKeyHolder::Add(const NodeId& peer, const asymm::PublicKey& public_key) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (std::any_of(std::begin(elements_), std::end(elements_),
                  [&](const PublicKeyInfo& info) {
                    return info.peer == peer;
                  }))
    return false;
  auto timer(std::make_shared<boost::asio::deadline_timer>(
      io_service_.service(), boost::posix_time::seconds(Parameters::public_key_holding_time)));
  timer->async_wait([peer, this](const boost::system::error_code& error) {
                      {
                        std::lock_guard<std::mutex> lock(mutex_);
                        this->elements_.erase(std::remove_if(
                            std::begin(this->elements_), std::end(this->elements_),
                            [peer](const PublicKeyInfo& info) {
                              return info.peer == peer;
                            }), std::end(this->elements_));
                      }
                      if (!error) {
                        this->network_.Remove(peer);
                      }
                    });
  elements_.emplace_back(PublicKeyInfo(peer, public_key, timer));
  return true;
}

boost::optional<asymm::PublicKey> PublicKeyHolder::Find(const NodeId& peer) const {
  boost::optional<asymm::PublicKey> public_key;

  std::lock_guard<std::mutex> lock(mutex_);
  auto iter(std::find_if(std::begin(elements_), std::end(elements_),
                         [&](const PublicKeyInfo& info) {
                           return info.peer == peer;
                         }));
  if (iter != std::end(elements_))
    public_key.reset(iter->public_key);
  return public_key;
}

void PublicKeyHolder::Remove(const NodeId& peer) {
  TimerPointer timer;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto element(std::find_if(std::begin(elements_), std::end(elements_),
                              [&](const PublicKeyInfo& info) {
                                return info.peer == peer;
                              }));

    if (element != std::end(elements_)) {
      timer = element->timer;
    }
  }
  timer->cancel();
}

}  // namespace routing

}  // namespace maidsafe
