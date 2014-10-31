/*  Copyright 2014 MaidSafe.net limited

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

#include <tuple>

#include "maidsafe/routing/receiver.h"
#include "maidsafe/routing/sender.h"
#include "maidsafe/routing/.h"

#ifndef MAIDSAFE_ROUTING_RECEIVER_H_
#define MAIDSAFE_ROUTING_RECEIVER_H_

namespace maidsafe {
namespace routing {

template <typename Message>
class Receiver {
 public:
  Receiver(RoutingTable routing_table, rudp::ManagedConnections managed_connections, Sender sender)
      : routing_table_(routing_table), managed_connections_(managed_connections), sender_(sender) {}

  Receiver(Receiver const&) = default;
  Receiver(Receiver&&) = default MAIDSAFE_NOEXCEPT;
  ~Receiver() = default;
  Receiver& operator=(Receiver const&) = default;
  Receiver& operator=(Receiver&&) = default MAIDSAFE_NOEXCEPT;

  friend bool operator==(const Receiver& other) const MAIDSAFE_NOEXCEPT {
    return std::tie(routing_table_, managed_connections_, sender_) ==
           std::tie(other.routing_table_, other.managed_connections_, other.sender_);
  }
  friend bool operator!=(const Receiver& other) const MAIDSAFE_NOEXCEPT {
    return !operator==(*this, other);
  }
  friend bool operator<(const Receiver& other) const MAIDSAFE_NOEXCEPT {
    return std::tie(routing_table_, managed_connections_, sender_) <
           std::tie(other.routing_table_, other.managed_connections_, other.sender_);
  }
  friend bool operator>(const Receiver& other) { return operator<(other, *this); }
  friend bool operator<=(const Receiver& other) { return !operator>(*this, other); }
  friend bool operator>=(const Receiver& other) { return !operator<(*this, other); }

 private:
  RoutingTable routing_table_;
  rudp::ManagedConnections managed_connections_;
  Sender sender_;
};



}  // namespace routing
}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_RECEIVER_H_

