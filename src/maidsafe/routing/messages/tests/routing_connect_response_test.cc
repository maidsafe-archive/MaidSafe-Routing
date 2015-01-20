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

#include "maidsafe/routing/messages/connect_response.h"

#include "maidsafe/common/serialisation/binary_archive.h"
#include "maidsafe/common/serialisation/serialisation.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/rudp/contact.h"

#include "maidsafe/routing/messages/messages_fwd.h"
#include "maidsafe/routing/tests/utils/test_utils.h"
#include "maidsafe/routing/messages/tests/generate_message_header.h"

namespace maidsafe {

namespace routing {

namespace test {

namespace {

ConnectResponse GenerateInstance() {
  return {rudp::EndpointPair{GetRandomEndpoint(), GetRandomEndpoint()},
          rudp::EndpointPair{GetRandomEndpoint(), GetRandomEndpoint()},
          Address{RandomString(Address::kSize)}, Address{RandomString(Address::kSize)},
          Address{RandomString(Address::kSize)}};
}

}  // anonymous namespace

TEST(ConnectResponseTest, BEH_SerialiseParse) {
  // Serialise
  auto connect_resp_before(GenerateInstance());
  auto header_before(GenerateMessageHeader());
  auto tag_before(MessageToTag<ConnectResponse>::value());

  auto serialised_connect_rsp(Serialise(header_before, tag_before, connect_resp_before));

  // Parse
  auto connect_resp_after(GenerateInstance());
  auto header_after(GenerateMessageHeader());
  auto tag_after(MessageTypeTag{});

  InputVectorStream binary_input_stream{serialised_connect_rsp};

  // Parse Header, Tag
  Parse(binary_input_stream, header_after, tag_after);

  EXPECT_EQ(header_before, header_after);
  EXPECT_EQ(tag_before, tag_after);

  // Parse the rest
  Parse(binary_input_stream, connect_resp_after);

  EXPECT_EQ(connect_resp_before.requester_endpoints, connect_resp_after.requester_endpoints);
  EXPECT_EQ(connect_resp_before.receiver_endpoints, connect_resp_after.receiver_endpoints);

  EXPECT_EQ(connect_resp_before.requester_id, connect_resp_after.requester_id);
  EXPECT_EQ(connect_resp_before.receiver_id, connect_resp_after.receiver_id);

  EXPECT_TRUE(!!connect_resp_before.relay_node);
  EXPECT_TRUE(!!connect_resp_after.relay_node);
  EXPECT_EQ(*connect_resp_before.relay_node, *connect_resp_after.relay_node);
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
