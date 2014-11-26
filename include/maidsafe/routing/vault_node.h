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

class vault_node : private rudp::managed_connections::listener {
 public:
  vault_node(AsioService& io_service, rudp::managed_connections managed_connections,
             const passport::Pmid& pmid);

  vault_node(const vault_node&) = delete;
  vault_node(vault_node&&) = delete;
  vault_node& operator=(const vault_node&) = delete;
  vault_node& operator=(vault_node&&) = delete;

  ~vault_node();

  // Used for bootstrapping (joining) and can be used as zero state network if both ends are started
  // simultaneously or to connect to a specific vault_node.
  void bootstrap(const endpoint& our_endpoint, const endpoint& their_endpoint,
                 const asymm::PublicKey& their_public_key);
  // Use hard coded vault_nodes or cache file
  void bootstrap();


  const NodeId our_id() const;

  // Returns a number between 0 to 100 representing % network health w.r.t. number of connections
  int network_status() const;

 private:
  AsioService& asio_service_;
  rudp::managed_connections rudp_;
  const NodeId our_id_;
  const asymm::Keys keys_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ROUTING_H_
