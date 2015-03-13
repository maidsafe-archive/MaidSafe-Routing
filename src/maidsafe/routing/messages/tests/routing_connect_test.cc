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

#include "maidsafe/routing/messages/connect.h"

#include "maidsafe/common/serialisation/binary_archive.h"
#include "maidsafe/common/serialisation/serialisation.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/passport/passport.h"

#include "maidsafe/routing/contact.h"
#include "maidsafe/routing/message_header.h"
#include "maidsafe/routing/messages/messages_fwd.h"
#include "maidsafe/routing/tests/utils/test_utils.h"

namespace maidsafe {

namespace routing {

namespace test {

namespace {

Connect GenerateInstance() {
  return Connect{EndpointPair{GetRandomEndpoint(), GetRandomEndpoint()},
                 MakeIdentity(), MakeIdentity(),
                 passport::PublicPmid(passport::Pmid(passport::Anpmid()))};
}

}  // anonymous namespace


TEST(ConnectTest, BEH_SerialiseParse) {
  // Serialise
  auto connect_before(GenerateInstance());
  auto header_before(GetRandomMessageHeader());
  auto tag_before(MessageToTag<Connect>::value());

  auto serialised_connect(Serialise(header_before, tag_before, connect_before));

  // Parse
  auto connect_after(GenerateInstance());
  auto header_after(GetRandomMessageHeader());
  auto tag_after(MessageTypeTag{});

  InputVectorStream binary_input_stream{serialised_connect};

  // Parse Header, Tag
  Parse(binary_input_stream, header_after, tag_after);

  EXPECT_EQ(header_before, header_after);
  EXPECT_EQ(tag_before, tag_after);

  // Parse the rest
  Parse(binary_input_stream, connect_after);

  EXPECT_EQ(connect_before.requester_endpoints(), connect_after.requester_endpoints());
  EXPECT_EQ(connect_before.requester_id(), connect_after.requester_id());
  EXPECT_EQ(connect_before.receiver_id(), connect_after.receiver_id());
  EXPECT_EQ(connect_before.requester_fob().validation_token(),
            connect_after.requester_fob().validation_token());
  EXPECT_TRUE(asymm::MatchingKeys(connect_before.requester_fob().public_key(),
                                  connect_after.requester_fob().public_key()));
  auto moved_connect(std::move(connect_after));
  EXPECT_EQ(connect_before.requester_endpoints(), moved_connect.requester_endpoints());
  EXPECT_EQ(connect_before.requester_endpoints(), moved_connect.requester_endpoints());
  EXPECT_EQ(connect_before.requester_id(), moved_connect.requester_id());
  EXPECT_EQ(connect_before.receiver_id(), moved_connect.receiver_id());
  EXPECT_EQ(connect_before.requester_fob().validation_token(),
            moved_connect.requester_fob().validation_token());
  EXPECT_TRUE(asymm::MatchingKeys(connect_before.requester_fob().public_key(),
                                  moved_connect.requester_fob().public_key()));
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
