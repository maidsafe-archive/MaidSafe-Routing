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

#include <vector>
#include <string>

#include "eggs/variant.hpp"
#include "boost/expected/expected.hpp"
#include "maidsafe/common/test.h"
#include "maidsafe/common/error.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

namespace test {

TEST(RoutingVariantTest, BEH_ExpextedVariant) {
  boost::expected<eggs::variant<int, std::string> const, maidsafe_error> v(42);
  boost::expected<eggs::variant<int, std::string>, maidsafe_error> x(std::string("hello"));
  EXPECT_TRUE(bool(v));
  EXPECT_TRUE(bool(*v));
  EXPECT_TRUE(bool(x));
  *x = 32;
  EXPECT_TRUE(bool(x));
  ASSERT_EQ(*x->target<int>(), 32);
  EXPECT_EQ(x->target_type(), typeid(int));
  *x = std::string("hello again");
  EXPECT_EQ(x->target_type(), typeid(std::string));

  ASSERT_EQ(*x->target<std::string>(), std::string("hello again"));

  EXPECT_EQ(v->which(), 0u);
  EXPECT_EQ(x->which(), 1u);
  ASSERT_EQ(*v->target<int>(), 42);
  ASSERT_EQ(*x->target<std::string>(), std::string("hello again"));

  int const& ref = eggs::variants::get<0>(*v);

  EXPECT_EQ(ref, 42);

  EXPECT_THROW(eggs::variants::get<1>(*v), eggs::variants::bad_variant_access);
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
