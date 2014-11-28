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
#ifndef MAIDSAFE_ROUTING_NODE_COMMON_H_
#define MAIDSAFE_ROUTING_NODE_COMMON_H_

#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/common/asio_service.h"
#include "maidsafe/routing/connnection_manager.h"

struct node_common {
  node_common() = default;
  node_common(node_common const&) = default;
  node_common(node_common&&) MAIDSAFE_NOEXCEPT : rudp(std::move(rhs.rudp)),
                                                 io_service(std::move(rhs.io_service)),
                                                 message_handle(std::move(rhs.message_handle)) {}
  ~node_common() = default;
  node_common& operator=(node_common const&) = default;
  node_common& operator=(node_common&& rhs) MAIDSAFE_NOEXCEPT {
    rudp = std::move(rhs.rudp), io_service = std::move(rhs.io_service),
    message_handle = std::move(rhs.message_handle)
  }

  void send(destination_id destination, std::string message);
  void send_direct(destination_id destination, std::string message);



  bool operator==(const node_common& other) const {
    return std::tie(rudp, io_service, message_handle) ==
      std::tie(other.rudp, other.io_service, other.message_handle);
  }
  bool operator!=(const node_common& other) const { 
    return !operator==(*this, other); 
  }
  bool operator<(const node_common& other) const {
    return std::tie(rudp, io_service, message_handle) <
      std::tie(other.rudp, other.io_service, other.message_handle);
  }  
  bool operator>(const node_common& other) {
    return operator<(other, *this);
  }
  bool operator<=(const node_common& other) {
    return !operator>(*this, other);
  }
  bool operator>=(const node_common& other) {
    return !operator<(*this, other);
  }   

  rudp::managed_connecitons&  rudp{};
  AsioService&  io_service{};
  message_handler  message_handle{};
  
};

#endif  // MAIDSAFE_ROUTING_NODE_COMMON_H_
 
