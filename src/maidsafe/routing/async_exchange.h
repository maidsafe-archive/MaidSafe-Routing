/*  Copyright 2012 MaidSafe.net limited

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

#ifndef MAIDSAFE_ROUTING_ASYNC_EXCHANGE_H_
#define MAIDSAFE_ROUTING_ASYNC_EXCHANGE_H_

#include "boost/optional/optional.hpp"

#include "maidsafe/crux/socket.hpp"

#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

template <class Handler /* void(boost::system::error_code, SerialisedMessage) */>
void AsyncExchange(crux::socket& socket, SerialisedMessage our_data, Handler handler) {
  // TODO(Team): Use some predefined constant for buffer size.
  static const std::size_t max_buffer_size = 262144;

  struct State {
    boost::optional<boost::system::error_code> first_error;
    SerialisedMessage rx_buffer;
    SerialisedMessage tx_buffer;
  };

  auto state = make_shared<State>();

  state->rx_buffer.resize(max_buffer_size);
  state->tx_buffer = std::move(our_data);

  socket.async_send(boost::asio::buffer(state->tx_buffer),
                    [state, handler](boost::system::error_code error, std::size_t) {
    if (state->first_error) {
      if (*state->first_error) {
        return handler(*state->first_error, SerialisedMessage());
      }
      if (error) {
        return handler(error, SerialisedMessage());
      }
      return handler(error, std::move(state->rx_buffer));
    } else {
      state->first_error = error;
    }
  });

  socket.async_receive(boost::asio::buffer(state->rx_buffer),
                       [state, handler](boost::system::error_code error, std::size_t size) {
    if (state->first_error) {
      if (*state->first_error) {
        return handler(*state->first_error, SerialisedMessage());
      }
      if (error) {
        return handler(error, SerialisedMessage());
      }
      state->rx_buffer.resize(size);
      return handler(error, std::move(state->rx_buffer));
    } else {
      state->first_error = error;
    }
  });
}

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ASYNC_EXCHANGE_H_
