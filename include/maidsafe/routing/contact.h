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

#ifndef MAIDSAFE_ROUTING_CONTACT_H_
#define MAIDSAFE_ROUTING_CONTACT_H_

#include "maidsafe/common/config.h"
#include "maidsafe/common/identity.h"
#include "maidsafe/common/rsa.h"
#include "maidsafe/routing/endpoint_pair.h"

namespace maidsafe {

namespace routing {

struct Contact {
  Contact() = default;
  Contact(const Contact&) = default;
  Contact(Contact&& other) MAIDSAFE_NOEXCEPT : id(std::move(other.id)),
                                               endpoint_pair(std::move(other.endpoint_pair)),
                                               public_key(std::move(other.public_key)) {}
  Contact& operator=(const Contact&) = default;
  Contact& operator=(Contact&& other) {
    id = std::move(other.id);
    endpoint_pair = std::move(other.endpoint_pair);
    public_key = std::move(other.public_key);
    return *this;
  }

  Contact(Identity node_id, EndpointPair::Endpoint both, asymm::PublicKey public_key_in)
      : id(std::move(node_id)),
        endpoint_pair(std::move(both)),
        public_key(std::move(public_key_in)) {}

  Contact(Identity node_id, EndpointPair endpoint_pair_in, asymm::PublicKey public_key_in)
      : id(std::move(node_id)),
        endpoint_pair(std::move(endpoint_pair_in)),
        public_key(std::move(public_key_in)) {}

  Identity id;
  EndpointPair endpoint_pair;
  asymm::PublicKey public_key;
};

#ifndef NDEBUG
inline std::ostream& operator<<(std::ostream& os, const Contact& c) {
  return os << "(Contact " << c.id << " " << c.endpoint_pair << ")";
}
#endif

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_CONTACT_H_
