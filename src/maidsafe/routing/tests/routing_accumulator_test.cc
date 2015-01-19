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

#include "maidsafe/routing/accumulator.h"

#include <chrono>
#include <thread>

#include "maidsafe/common/test.h"
#include "maidsafe/common/node_id.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/routing/types.h"


namespace maidsafe {

namespace routing {

namespace test {

TEST(RoutingTest, BEH_AccumulatorAdd) {
  auto quorum(1U);
  Accumulator<int, uint32_t> accumulator(std::chrono::milliseconds(10), quorum);
  auto test_address(Address{RandomString(Address::kSize)});
  EXPECT_TRUE(!!accumulator.Add(2, 3UL, test_address));
  EXPECT_FALSE(accumulator.HaveKey(1));
  EXPECT_TRUE(accumulator.HaveKey(2));
  EXPECT_FALSE(accumulator.CheckQuorumReached(1));
  EXPECT_TRUE(accumulator.CheckQuorumReached(2));
  EXPECT_TRUE(!!accumulator.Add(1, 3UL, Address{RandomString(Address::kSize)}));
  EXPECT_TRUE(accumulator.HaveKey(1));
  EXPECT_TRUE(accumulator.CheckQuorumReached(1));
  EXPECT_TRUE(!!accumulator.Add(1, 3UL, Address{RandomString(Address::kSize)}));
  EXPECT_TRUE(accumulator.CheckQuorumReached(1));
  EXPECT_EQ(accumulator.GetAll(1)->second.size(), 2);
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  EXPECT_TRUE(accumulator.CheckQuorumReached(1));
  EXPECT_TRUE(!!accumulator.GetAll(1));
  EXPECT_TRUE(accumulator.HaveKey(1));
  EXPECT_TRUE(accumulator.CheckQuorumReached(2));
  EXPECT_TRUE(!!accumulator.GetAll(2));
  EXPECT_TRUE(accumulator.HaveKey(2));
  //  this Add will check timers of all other keys and should remove them as they are now timed
  //  out
  EXPECT_TRUE(!!accumulator.Add(3, 3UL, test_address));
  EXPECT_FALSE(accumulator.CheckQuorumReached(1));
  EXPECT_FALSE(!!accumulator.GetAll(1));
  EXPECT_FALSE(accumulator.HaveKey(1));
  EXPECT_FALSE(accumulator.CheckQuorumReached(2));
  EXPECT_FALSE(!!accumulator.GetAll(2));
  EXPECT_FALSE(accumulator.HaveKey(2));
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
