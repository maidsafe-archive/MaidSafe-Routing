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

#include "maidsafe/routing/messages/forward_connect.h"

#include "maidsafe/common/serialisation/binary_archive.h"
#include "maidsafe/routing/compile_time_mapper.h"
#include "maidsafe/common/serialisation/serialisation.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/rudp/contact.h"

#include "maidsafe/routing/messages/messages_fwd.h"
#include "maidsafe/routing/tests/utils/test_utils.h"

namespace maidsafe {

namespace routing {

namespace test {

namespace {

ForwardConnect GenerateInstance() {
  const auto destination_id = DestinationAddress{Address{RandomString(Address::kSize)}};
  const auto source_id = SourceAddress{Address{RandomString(Address::kSize)}};
  const auto endpoints = rudp::EndpointPair{GetRandomEndpoint(), GetRandomEndpoint()};
  const auto receiver_id = Address{RandomString(Address::kSize)};
  const auto requester_id = Address{RandomString(Address::kSize)};

  return ForwardConnect {
    Connect {destination_id, source_id, endpoints, receiver_id},
    source_id
  };
}

}  // anonymous namespace

TEST(ForwardConnectTest, BEH_SerialiseParse) {
  // Serialise
  ForwardConnect fwd_connct_before {GenerateInstance()};
  auto tag_before = GivenTypeFindTag_v<ForwardConnect>::value;

  auto serialised_fwd_cnnct = Serialise(tag_before, fwd_connct_before);

  // Parse
  ForwardConnect fwd_connct_after {GenerateInstance()};
  auto tag_after = MessageTypeTag{};

  InputVectorStream binary_input_stream{serialised_fwd_cnnct};

  // Parse Tag
  Parse(binary_input_stream, tag_after);

  EXPECT_EQ(tag_before, tag_after);

  // Parse the rest
  Parse(binary_input_stream, fwd_connct_after);

  EXPECT_EQ(fwd_connct_before.header, fwd_connct_after.header);
  EXPECT_EQ(fwd_connct_before.requester.endpoint_pair.local,
            fwd_connct_after.requester.endpoint_pair.local);
  EXPECT_EQ(fwd_connct_before.requester.endpoint_pair.external,
            fwd_connct_after.requester.endpoint_pair.external);
  EXPECT_EQ(fwd_connct_before.requester.id, fwd_connct_after.requester.id);
  EXPECT_EQ(fwd_connct_before.requester.public_key, fwd_connct_after.requester.public_key);
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
