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
#include "maidsafe/common/serialisation/compile_time_mapper.h"
#include "maidsafe/common/serialisation/serialisation.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/rudp/contact.h"

#include "maidsafe/routing/messages/messages_fwd.h"
#include "maidsafe/routing/tests/utils/test_utils.h"

namespace maidsafe {

namespace routing {

namespace test {

TEST(ConnectTest, BEH_SerialiseParse) {
  const auto destination_id = DestinationAddress{Address{RandomString(Address::kSize)}};
  const auto source_id = SourceAddress{Address{RandomString(Address::kSize)}};
  const auto endpoints = rudp::EndpointPair{GetRandomEndpoint(), GetRandomEndpoint()};
  const auto receiver_id = Address{RandomString(Address::kSize)};

  // Serialise
  auto connect = Connect{destination_id, source_id, endpoints, receiver_id};
  auto serialised_connect = Serialise(std::move(connect));

  // Parse header and tag
  auto header = MessageHeader{};
  auto tag = SerialisableTypeTag{0};
  InputVectorStream binary_input_stream{serialised_connect};
  {
    BinaryInputArchive binary_input_archive(binary_input_stream);
    binary_input_archive(header, tag);
  }
  EXPECT_EQ(destination_id, header.destination);
  EXPECT_EQ(source_id, header.source);
  EXPECT_EQ(MessageTypeTag::kConnect, static_cast<MessageTypeTag>(tag));

  // Parse remainder
  auto parsed_connect = Connect{std::move(header)};
  {
    BinaryInputArchive binary_input_archive(binary_input_stream);
    binary_input_archive(parsed_connect);
  }
  EXPECT_EQ(endpoints.local, parsed_connect.requester_endpoints.local);
  EXPECT_EQ(endpoints.external, parsed_connect.requester_endpoints.external);
  EXPECT_EQ(receiver_id, parsed_connect.receiver_id);
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
