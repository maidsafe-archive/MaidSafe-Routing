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

#ifndef MAIDSAFE_ROUTING_HEADER_H_
#define MAIDSAFE_ROUTING_HEADER_H_

#include "maidsafe/common/config.h"

#include "maidsafe/routing/types.h"
#include "maidsafe/routing/utils.h"

namespace maidsafe {

namespace routing {

// For use with small messages which aren't scatter/gathered
template <typename Destination, typename Source>
struct small_header {
  small_header() = default;
  small_header(const small_header&) = delete;
  small_header(small_header&&) MAIDSAFE_NOEXCEPT = default;
  small_header(Destination destination_in, Source source_in, message_id message_id_in,
               murmur_hash checksum_in)
      : destination(std::move(destination_in)),
        source(std::move(source_in)),
        message_id(std::move(message_id_in)),
        checksum(std::move(checksum_in)) {}
  ~small_header() = default;
  small_header& operator=(const small_header&) = delete;
  small_header& operator=(small_header&&) MAIDSAFE_NOEXCEPT = default;

  template <typename Archive>
  void serialize(Archive& archive) {
    archive(destination, source, message_id, checksum);
  }

  Destination destination;
  Source source;
  message_id message_id;
  murmur_hash checksum;
};

// For use with scatter/gather messages
template <typename Destination, typename Source>
struct header {
  header() = default;
  header(const header&) = delete;
  header(header&&) MAIDSAFE_NOEXCEPT = default;
  header(Destination destination_in, Source source_in, message_id message_id_in,
         murmur_hash payload_checksum_in, checksums other_checksum_in)
      : basic_info(std::move(destination_in), std::move(source_in), std::move(message_id_in),
                   std::move(payload_checksum_in)),
        other_checksums(std::move(other_checksum_in)) {}
  ~header() = default;
  header& operator=(const header&) = delete;
  header& operator=(header&&) MAIDSAFE_NOEXCEPT = default;

  template <typename Archive>
  void serialize(Archive& archive) {
    archive(basic_info, other_checksums);
  }

  small_header<Destination, Source> basic_info;
  checksums other_checksums;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_HEADER_H_
