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

#ifndef MAIDSAFE_ROUTING_TESTS_UTILS_ROUTING_TABLE_UNIT_TEST_H_
#define MAIDSAFE_ROUTING_TESTS_UTILS_ROUTING_TABLE_UNIT_TEST_H_

#include <array>
#include <cstdint>
#include <vector>

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/test.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/tests/utils/test_utils.h"

#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

namespace test {

class RoutingTableUnitTest : public testing::Test {
 public:
  struct Bucket {
    Bucket() = default;
    Bucket(const Address& furthest_from_tables_own_id, unsigned index_in);
    unsigned index;
    Address far_contact, mid_contact, close_contact;
  };

 protected:
  using Buckets = std::array<Bucket, 100>;

  RoutingTableUnitTest();

  void PartiallyFillTable();
  void CompleteFillingTable();

  const Address our_id_;
  const passport::Pmid fob_;
  const passport::PublicPmid public_fob_;
  RoutingTable table_;
  const Buckets buckets_;
  NodeInfo info_;
  const size_t initial_count_;
  std::vector<Address> added_ids_;

 private:
  Buckets InitialiseBuckets();
};

}  // namespace test

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_TESTS_UTILS_ROUTING_TABLE_UNIT_TEST_H_
