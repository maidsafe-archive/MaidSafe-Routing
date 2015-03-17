/*  Copyright 2015 MaidSafe.net limited

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

#ifndef MAIDSAFE_ROUTING_ASYNC_QUEUE_H_
#define MAIDSAFE_ROUTING_ASYNC_QUEUE_H_

#include <mutex>
#include <queue>
#include <tuple>

#include "asio/async_result.hpp"

#include "maidsafe/common/config.h"

namespace maidsafe {

namespace routing {

namespace detail {

namespace helper {

// For more info in ApplyTuple see: https://www.preney.ca/paul/archives/486

template <int... Is>
struct Index {
  using Next = Index<Is..., sizeof...(Is)>;
};

//template <int N, int... Is>
//struct GeneratedSequence : GeneratedSequence<N - 1, N - 1, Is...> {};
//
//template <int... Is>
//struct GeneratedSequence<0, Is...> : Index<Is...> {};

template<int Size> struct BuildIndices {
  using Type = typename BuildIndices<Size - 1>::Type::Next;
};

template<> struct BuildIndices<0> {
  using Type = Index<>;
};

}  // namespace helper

template <class F, typename Tuple, int... Is>
inline MAIDSAFE_CONSTEXPR void ApplyTuple(F&& f, Tuple&& tup, helper::Index<Is...>)
{ //-> decltype(f(std::get<Is>(tup)...)) {
  f(std::get<Is>(std::forward<Tuple>(tup))...);
}

template <class F, typename Tuple>
inline MAIDSAFE_CONSTEXPR void ApplyTuple(F&& f, Tuple&& tup)
//    -> decltype(ApplyTuple(f,
//                           tup,
//                           helper::GeneratedSequence<std::tuple_size<Tuple>::value>{})) {
 {
  ApplyTuple(std::forward<F>(f),
             std::forward<Tuple>(tup),
             typename helper::BuildIndices<std::tuple_size<typename std::decay<Tuple>::type>::value>::Type{});
}

}  // namespace detail

template <class... Args>
class AsyncQueue {
 private:
  using Handler = std::function<void(Args...)>;
  using Tuple = std::tuple<Args...>;

 public:
  template <class... Params>
  void Push(Params&&... args) {
    Handler handler;

    {
      std::lock_guard<std::mutex> lock(mutex);

      if (handlers.empty()) {
        return values.emplace(std::forward<Params>(args)...);
      }

      handler = std::move(handlers.front());
      handlers.pop();
    }

    handler(std::forward<Params>(args)...);
  }

  template <typename CompletionToken>
  typename asio::async_result<typename asio::handler_type<
      typename std::decay<CompletionToken>::type, void(Args...)>::type>::type
      AsyncPop(CompletionToken&& token) {
    using AsioHandler = typename asio::handler_type<typename std::decay<CompletionToken>::type,
                                                    void(Args...)>::type;

    AsioHandler handler = std::forward<decltype(token)>(token);

    asio::async_result<AsioHandler> result(handler);

    Tuple tuple;

    {
      std::lock_guard<std::mutex> lock(mutex);

      if (values.empty()) {
        handlers.emplace(std::move(handler));
        return result.get();
      }

      tuple = std::move(values.front());
      values.pop();
    }

    detail::ApplyTuple(handler, std::move(tuple));

    return result.get();
  }

 private:
  std::mutex mutex;
  std::queue<Handler> handlers;
  std::queue<Tuple> values;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ASYNC_QUEUE_H_
