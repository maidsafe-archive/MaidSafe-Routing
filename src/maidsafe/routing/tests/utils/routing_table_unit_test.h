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

#include "maidsafe/common/test.h"

#include <array>

#include "maidsafe/common/node_id.h"

#include "maidsafe/routing/routing_table.h"

namespace maidsafe {

namespace routing {

namespace test {

class routing_table_unit_test : public testing::Test {
 public:
  struct bucket {
    bucket() = default;
    bucket(const NodeId& furthest_from_tables_own_id, unsigned index_in);
    unsigned index;
    NodeId far_contact, mid_contact, close_contact;
  };

 protected:
  using buckets = std::array<bucket, 100>;

  routing_table_unit_test();

  routing_table table_;
  const buckets buckets_;

 private:
  buckets initialise_buckets();
};

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
