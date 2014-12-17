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

#include "maidsafe/routing/vault_node.h"

namespace maidsafe {

namespace routing {

VaultNode::VaultNode(asio::io_service& io_service, boost::filesystem::path db_location,
                     const passport::Pmid& pmid)
    : io_service_(io_service),
      rudp_(),
      bootstrap_handler_(std::move(db_location)),
      our_id_(pmid.name().value.string()),
      keys_([&pmid]() -> asymm::Keys {
        asymm::Keys keys;
        keys.private_key = pmid.private_key();
        keys.public_key = pmid.public_key();
        return keys;
      }()),
      rudp_listener_(std::make_shared<RudpListener>()) {}

}  // namespace routing

}  // namespace maidsafe
