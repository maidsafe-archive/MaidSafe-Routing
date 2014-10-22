/*  Copyright 2013 MaidSafe.net limited

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

// #include "maidsafe/routing/message.h"
//
// namespace maidsafe {
//
// namespace routing {
//
// // GroupSource::GroupSource() : group_id(), sender_id() {}
// //
// // GroupSource::GroupSource(GroupId group_id_in, SingleId sender_id_in)
// //     : group_id(std::move(group_id_in)), sender_id(std::move(sender_id_in)) {}
// //
// // GroupSource::GroupSource(const GroupSource& other)
// //     : group_id(other.group_id), sender_id(other.sender_id) {}
// //
// // GroupSource::GroupSource(GroupSource&& other)
// //     : group_id(std::move(other.group_id)), sender_id(std::move(other.sender_id)) {}
// //
// // GroupSource& GroupSource::operator=(GroupSource other) {
// //   swap(*this, other);
// //   return *this;
// // }
// //
// // void swap(GroupSource& lhs, GroupSource& rhs) {
// //   using std::swap;
// //   swap(lhs.group_id, rhs.group_id);
// //   swap(lhs.sender_id, rhs.sender_id);
// // }
// //
// // bool operator==(const GroupSource& lhs, const GroupSource& rhs) {
// //   return lhs.group_id == rhs.group_id &&
// //          lhs.sender_id == rhs.sender_id;
// // }
// // bool operator!=(const GroupSource& lhs, const GroupSource& rhs) {
// //   return !operator==(lhs, rhs);
// // }
// //
// // bool operator<(const GroupSource& lhs, const GroupSource& rhs) {
// //   return std::tie(lhs.group_id, lhs.sender_id)
// //     < std::tie(rhs.group_id, rhs.sender_id);
// // }
// //
// //
// // namespace detail {
// //
// // SingleIdRelay GetRelayIdToReply(const SingleRelaySource& single_relay_src) {
// //   return SingleIdRelay(SingleId(NodeId(single_relay_src.node_id->string())),
// //                        single_relay_src.connection_id,
// //                        SingleId(NodeId(single_relay_src.relay_node->string())));
// // }
// //
// // }  // namespace detail
//
// }  // namespace routing
//
// }  // namespace maidsafe
