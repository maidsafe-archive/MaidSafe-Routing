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
#include "maidsafe/passport/types.h"
#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

class node : private rudp::ManagedConnections::Listener {
 public:
  explicit node(const passport::Pmid& pmid);

  node(const node&) = delete;
  node(node&&) = delete;
  node& operator=(const node&) = delete;
  node& operator=(node&&) = delete;

  ~node();

  // Used for bootstrapping (joining) and can be used as zero state network if both ends are started
  // simultaneously or to connect to a specific node.
  void bootstrap(const our_endpoint& our_endpoint, const their_endpoint& their_endpoint,
                 const asymm::PublicKey& their_public_key);
  // Use hard coded nodes or cache file
  void bootstrap();

  template <typename CompletionToken>
  typename boost::asio::async_result<
      typename boost::asio::handler_type<CompletionToken, void(std::error_code)>::type>::type
      send(const NodeId& destination_id, const std::vector<unsigned char>& message, bool cacheable,
           CompletionToken&& token) {
    using handler_type =
        typename boost::asio::handler_type<CompletionToken, void(std::error_code)>::type;
    auto handler = handler_type{std::forward<decltype(token)>(token)};
    asio_service_.service().post([=]() mutable {
      do_send(std::move(destination_id), std::move(message), cacheable, std::move(handler));
    });
    return boost::asio::async_result<handler_type>(handler).get();
  }

  // Evaluates whether the sender_id is a legitimate source to send a request for performing an
  // operation on info_id
  bool estimate_in_group(const NodeId& sender_id, const NodeId& info_id) const;

  const NodeId our_id() const;

  // Returns a number between 0 to 100 representing % network health w.r.t. number of connections
  int network_status() const;

 private:
  template <typename CompletionHandler>
  void do_send(NodeId&& destination_id, std::vector<unsigned char>&& message, bool cacheable,
               CompletionHandler&& handler) {
    // form and send message(s)
    // once evaluating rudp's results invoke handler
    handler(make_error_code(CommonErrors::success));
  }

  AsioService asio_service_;
  const NodeId our_id_;
  const asymm::Keys keys_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ROUTING_H_
