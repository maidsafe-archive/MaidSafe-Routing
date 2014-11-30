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
    use of the MaidSafe Software.
 */

#ifndef MAIDSAFE_ROUTING_CONNECT_H_
#define MAIDSAFE_ROUTING_CONNECT_H_

#include "maidsafe/common/serialisation.h"
#include "maidsafe/common/node_id.h"


#include "maidsafe/routing/message_header.h"
#include "maidsafe/routing/types.h"

struct connect {
  connect() = default;
  connect(const connect&) = delete;
  connect(connect&& other) MAIDSAFE_NOEXCEPT
      : header(std::move(other.header)),
        requester_endpoints(std::move(other.requester_endpoints)),
        requester_id(std::move(other.requester_id)),
        receiver_id(std::move(other.receiver_id)) {}
  connect(destination_id destination_in, source_id source_in,
          rudp::endpoint_pair requester_endpoints, NodeId receiver_id, NodeId receiver_id)
      : header(std::move(destination_in), std::move(source_in), message_id(RandomUint32())),
        requester_endpoints(std::move(requester_endpoints)),
        requester_id(std::move(requester_id)),
        receiver_id(std::move(receiver_id)) {}
  ~connect() = default;
  connect& operator=(const connect&) = delete;
  connect& operator=(connect&& other) MAIDSAFE_NOEXCEPT {
    header = std::move(other.header);
    requester_endpoints = std::move(other.requester_endpoints);
    requester_id = std::move(other.requester_id);
    receiver_id = std::move(other.receiver_id);
    return *this;
  };

  template <typename Archive>
  void Serialise(Archive& archive) const {
    archive(header, requester_endpoints, requester_id, receiver_id);
  }

  header header;
  rudp::endpoint_pair requester_endpoints;
  NodeId requester_id;
  NodeId receiver_id;
};

#endif  // MAIDSAFE_ROUTING_CONNECT_H_
