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

#include "maidsafe/routing/node_info.h"

namespace maidsafe {

namespace routing {


// NodeInfo::NodeInfo(const serialised_type& serialised_message)
//     : connection_id(), public_key(), bucket(kInvalidBucket), nat_type(rudp::NatType::kUnknown) {
//   protobuf::NodeInfo proto_node_info;
//   if (!proto_node_info.ParseFromString(serialised_message->string()))
//     BOOST_THROW_EXCEPTION(MakeError(CommonErrors::parsing_error));
//
//   id = NodeId(proto_node_info.node_id());
//   rank = proto_node_info.rank();
// }
//
// NodeInfo::serialised_type NodeInfo::Serialise() const {
//   serialised_type serialised_message;
//   try {
//     protobuf::NodeInfo proto_node_info;
//     proto_node_info.set_node_id(id.string());
//     proto_node_info.set_rank(rank);
//
//     serialised_message = serialised_type(NonEmptyString(proto_node_info.SerializeAsString()));
//   } catch (const std::exception&) {
//     BOOST_THROW_EXCEPTION(MakeError(CommonErrors::invalid_parameter));
//   }
//
//   return serialised_message;
// }

const int32_t node_info::kInvalidBucket(std::numeric_limits<int32_t>::max());

}  // namespace routing

}  // namespace maidsafe
