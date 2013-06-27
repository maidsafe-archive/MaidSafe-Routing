/* Copyright 2012 MaidSafe.net limited

This MaidSafe Software is licensed under the MaidSafe.net Commercial License, version 1.0 or later,
and The General Public License (GPL), version 3. By contributing code to this project You agree to
the terms laid out in the MaidSafe Contributor Agreement, version 1.0, found in the root directory
of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also available at:

http://www.novinet.com/license

Unless required by applicable law or agreed to in writing, software distributed under the License is
distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
implied. See the License for the specific language governing permissions and limitations under the
License.
*/

#include "maidsafe/routing/node_info.h"

#include <limits>

#include "maidsafe/routing/routing.pb.h"

namespace maidsafe {

namespace routing {

NodeInfo::NodeInfo()
    : node_id(),
      connection_id(),
      public_key(),
      rank(),
      bucket(kInvalidBucket),
      nat_type(rudp::NatType::kUnknown),
      dimension_list() {}

NodeInfo::NodeInfo(const NodeInfo& other)
    : node_id(other.node_id),
      connection_id(other.connection_id),
      public_key(other.public_key),
      rank(other.rank),
      bucket(other.bucket),
      nat_type(other.nat_type),
      dimension_list(other.dimension_list) {}

NodeInfo& NodeInfo::operator=(const NodeInfo& other) {
  node_id = other.node_id;
  connection_id = other.connection_id;
  public_key = other.public_key;
  rank = other.rank;
  bucket = other.bucket;
  nat_type = other.nat_type;
  dimension_list = other.dimension_list;

  return *this;
}

NodeInfo::NodeInfo(NodeInfo&& other)
    : node_id(std::move(other.node_id)),
      connection_id(std::move(other.connection_id)),
      public_key(std::move(other.public_key)),
      rank(std::move(other.rank)),
      bucket(std::move(other.bucket)),
      nat_type(std::move(other.nat_type)),
      dimension_list(std::move(other.dimension_list)) {}

NodeInfo& NodeInfo::operator=(NodeInfo&& other) {
  node_id = std::move(other.node_id);
  connection_id = std::move(other.connection_id);
  public_key = std::move(other.public_key);
  rank = std::move(other.rank);
  bucket = std::move(other.bucket);
  nat_type = std::move(other.nat_type);
  dimension_list = std::move(other.dimension_list);

  return *this;
}

NodeInfo::NodeInfo(const serialised_type &serialised_message)
    : connection_id(),
      public_key(),
      bucket(kInvalidBucket),
      nat_type(rudp::NatType::kUnknown) {
  protobuf::NodeInfo proto_node_info;
  if (!proto_node_info.ParseFromString(serialised_message->string()))
    ThrowError(CommonErrors::parsing_error);

  node_id = NodeId(proto_node_info.node_id());
  rank = proto_node_info.rank();
  for (int i(0); i < proto_node_info.dimension_list_size(); ++i)
    dimension_list.push_back(proto_node_info.dimension_list(i));
}

NodeInfo::serialised_type NodeInfo::Serialise() const {
  serialised_type serialised_message;
  try {
    protobuf::NodeInfo proto_node_info;
    proto_node_info.set_node_id(node_id.string());
    proto_node_info.set_rank(rank);
    for (const auto& dimension : dimension_list)
      proto_node_info.add_dimension_list(dimension);

    serialised_message = serialised_type(NonEmptyString(proto_node_info.SerializeAsString()));
  }
  catch(const std::exception&) {
    ThrowError(CommonErrors::invalid_parameter);
  }

  return serialised_message;
}

const int32_t NodeInfo::kInvalidBucket(std::numeric_limits<int32_t>::max());

}  // namespace routing

}  // namespace maidsafe
