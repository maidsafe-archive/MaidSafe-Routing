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

#include "maidsafe/routing/messages/client_response.h"

#include "maidsafe/common/serialisation/binary_archive.h"
#include "maidsafe/routing/compile_time_mapper.h"
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

ClientResponse GenerateInstance() {
  const auto serialised_message(RandomString(Address::kSize));

  return {
    Address{RandomString(Address::kSize)},
    SerialisedData(serialised_message.begin(), serialised_message.end()),
    GenerateSHA1HashVector(),
    asymm::GenerateKeyPair().public_key
  };
}

}  // anonymous namespace

TEST(ClientResponseTest, BEH_SerialiseParse) {
  // Serialise
  auto fwd_resp_before(GenerateInstance());
  auto header_before(GenerateMessageHeader());
  auto tag_before(GivenTypeFindTag_v<ClientResponse>::value);

  auto serialised_fwd_resp(Serialise(header_before, tag_before, fwd_resp_before));

  // Parse
  auto fwd_resp_after(GenerateInstance());
  auto header_after(GenerateMessageHeader());
  auto tag_after(MessageTypeTag{});

  InputVectorStream binary_input_stream{serialised_fwd_resp};

  // Parse Header, Tag
  Parse(binary_input_stream, header_after, tag_after);

  EXPECT_EQ(header_before, header_after);
  EXPECT_EQ(tag_before, tag_after);

  // Parse the rest
  Parse(binary_input_stream, fwd_resp_after);

  EXPECT_EQ(fwd_resp_before.key, fwd_resp_after.key);

  EXPECT_EQ(fwd_resp_before.data.size(), fwd_resp_after.data.size());
  EXPECT_TRUE(std::equal(fwd_resp_before.data.begin(), fwd_resp_before.data.end(),
                         fwd_resp_after.data.begin()));

  EXPECT_EQ(fwd_resp_before.checksum.size(), fwd_resp_after.checksum.size());
  EXPECT_TRUE(std::equal(fwd_resp_before.checksum.begin(), fwd_resp_before.checksum.end(),
                         fwd_resp_after.checksum.begin()));

  EXPECT_EQ(rsa::EncodeKey(fwd_resp_before.requesters_public_key),
            rsa::EncodeKey(fwd_resp_after.requesters_public_key));
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe