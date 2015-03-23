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

#ifndef MAIDSAFE_ROUTING_TIMER_H_
#define MAIDSAFE_ROUTING_TIMER_H_

#include <chrono>
#include <functional>

#include "asio/steady_timer.hpp"

namespace maidsafe {

namespace routing {

class Timer {
 public:
  Timer(asio::io_service&);
  ~Timer();

  template <class Handler>
  void async_wait(asio::steady_timer::duration, Handler&&);

  void cancel();

 private:
  enum State { idle, running, canceled };
  asio::steady_timer timer_;
  std::shared_ptr<State> state_;
};

inline Timer::Timer(asio::io_service& ios) : timer_(ios), state_(std::make_shared<State>(idle)) {}

inline Timer::~Timer() { cancel(); }

template <class Handler>
void Timer::async_wait(asio::steady_timer::duration duration, Handler&& handler) {
  auto state = state_;
  assert(*state == idle && "Current implementation allows only one async_wait invocation");
  *state = running;
  timer_.expires_from_now(duration);
  // FIXME(Team): forward the handler if we're using c++14
  timer_.async_wait([state, handler](const asio::error_code&) {
    if (*state != running) {
      return;
    }
    handler();
  });
}

inline void Timer::cancel() {
  *state_ = canceled;
  timer_.cancel();
}

}  // routing namespace

}  // maidsafe namespace

#endif  // MAIDSAFE_ROUTING_TIMER_H_
