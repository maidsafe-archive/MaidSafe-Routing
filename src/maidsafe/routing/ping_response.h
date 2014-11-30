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

#ifndef MAIDSAFE_ROUTING_PING_REPONSE_H_
#define MAIDSAFE_ROUTING_PING_REPONSE_H_

#include "maidsafe/common/serialisation.h"

#include "maidsafe/routing/message_header.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/ping.h"

struct ping_response {
  static const message_type_tag message_type = message_type_tag::ping_response;

  ping_response() = default;
  ping_response(const ping_response&) = delete;
  ping_response(ping_response&& other) MAIDSAFE_NOEXCEPT : header(std::move(other.header)) {}
  explicit ping_response(ping ping)
      : header(destination_id(std::move(ping.header.source.data)),
               source_id(std::move(ping.header.destination.data)),
               message_id(std::move(ping.header.message_id))) {}
  ~ping_response() = default;
  ping_response& operator=(const ping_response&) = delete;
  ping_response& operator=(ping_response&& other) MAIDSAFE_NOEXCEPT {
    header = std::move(other.header);
    return *this;
  };

  template <typename Archive>
  void Serialise(Archive& archive) const {
    archive(header);
  }

  header header;
};


#endif  // MAIDSAFE_ROUTING_PING_REPONSE_H_
