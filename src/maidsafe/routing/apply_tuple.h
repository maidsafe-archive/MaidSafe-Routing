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

#ifndef MAIDSAFE_ROUTING_APPLY_TUPLE_H_
#define MAIDSAFE_ROUTING_APPLY_TUPLE_H_

#include <tuple>

namespace maidsafe {

namespace routing {

namespace helper {

template <int... Is>
struct Index {};

template <int N, int... Is>
struct GeneratedSequence : GeneratedSequence<N - 1, N - 1, Is...> {};

template <int... Is>
struct GeneratedSequence<0, Is...> : Index<Is...> {};

template <class F, typename... Args, int... Is>
inline MAIDSAFE_CONSTEXPR auto ApplyTuple(F&& f, const std::tuple<Args...>& tup,
                                          helper::Index<Is...>)
    -> decltype(f(std::get<Is>(tup)...)) {
  return f(std::get<Is>(tup)...);
}

}  // namespace helper

template <class F, typename... Args>
inline MAIDSAFE_CONSTEXPR auto ApplyTuple(F&& f, const std::tuple<Args...>& tup)
    -> decltype(helper::ApplyTuple(std::forward<F>(f), tup,
                                   helper::GeneratedSequence<sizeof...(Args)>{})) {
  return helper::ApplyTuple(std::forward<F>(f), tup, helper::GeneratedSequence<sizeof...(Args)>{});
}

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_APPLY_TUPLE_H_
