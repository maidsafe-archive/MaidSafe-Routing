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

#ifndef MAIDSAFE_ROUTING_ROUTING_H_
#define MAIDSAFE_ROUTING_ROUTING_H_

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/asio_service.h"
#include "maidsafe/passport/types.h"
#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

class VaultNode : private rudp::ManagedConnections::Listener {
 public:
  VaultNode(AsioService& io_service, rudp::ManagedConnections managed_connections,
             const passport::Pmid& pmid);

  VaultNode(const VaultNode&) = delete;
  VaultNode(VaultNode&&) = delete;
  VaultNode& operator=(const VaultNode&) = delete;
  VaultNode& operator=(VaultNode&&) = delete;

  ~VaultNode();

  // Used for bootstrapping (joining) and can be used as zero state network if both ends are started
  // simultaneously or to connect to a specific VaultNode.
  void Bootstrap(const our_endpoint& our_endpoint, const their_endpoint& their_endpoint,
                 const asymm::PublicKey& their_public_key);
  // Use hard coded VaultNodes or cache file
  void Bootstrap();


  const NodeId OurId() const;

  // Returns a number between 0 to 100 representing % network health w.r.t. number of connections
  int NetworkStatus() const;

 private:
  AsioServicei& asio_service_;
  rudp::ManagedConnections rudp_;
  const NodeId our_id_;
  const asymm::Keys keys_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ROUTING_H_
