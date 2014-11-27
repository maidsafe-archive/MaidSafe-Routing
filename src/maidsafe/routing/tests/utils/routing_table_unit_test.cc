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

#include "maidsafe/routing/tests/utils/routing_table_unit_test.h"

#include <bitset>
#include <string>

#include "maidsafe/common/utils.h"

namespace maidsafe {

namespace routing {

namespace test {

namespace {

enum class contact_type { far_out, mid, close };

NodeId get_contact(const NodeId& furthest_from_tables_own_id, unsigned bucket_index,
                   contact_type the_contact_type) {
  std::bitset<NodeId::kSize * 8> binary_id{
      furthest_from_tables_own_id.ToStringEncoded(NodeId::EncodingType::kBinary)};
  if (bucket_index > 0)
    binary_id.flip((NodeId::kSize * 8) - bucket_index);
  switch (the_contact_type) {
    case contact_type::mid:
      id.flip(0);
      break;
    case contact_type::close:
      id.flip(1);
      break;
    case contact_type::far_out:  // no change to binary_id
    default:
      break;
  }
  return NodeId{binary_id.to_string(), NodeId::EncodingType::kBinary};
}

}  // unnamed namespace
routing_table_unit_test::bucket::bucket(const NodeId& furthest_from_tables_own_id,
                                        unsigned index_in)
    : index(index_in),
      far_contact(get_contact(furthest_from_tables_own_id, index, contact_type::far_out)),
      mid_contact(get_contact(furthest_from_tables_own_id, index, contact_type::mid)),
      close_contact(get_contact(furthest_from_tables_own_id, index, contact_type::close)) {}

routing_table_unit_test::routing_table_unit_test()
    : table_(NodeId{RandomString(NodeId::kSize)}),
      furthest_id_(table_.our_id() ^ NodeId(std::string(NodeId::kSize, -1))),
      buckets_(initialise_buckets(furthest_id_)) {}

routing_table_unit_test::buckets routing_table_unit_test::initialise_buckets(
    const NodeId& furthest_from_tables_own_id) {
  buckets the_buckets;
  for (int i = 0; i < the_buckets.size(); ++i)
    the_buckets[i] = bucket{furthest_from_tables_own_id, i};
  return the_buckets;
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
