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

#include "maidsafe/routing/messages/forward_put_data.h"

#include "maidsafe/common/serialisation/binary_archive.h"
#include "maidsafe/routing/compile_time_mapper.h"
#include "maidsafe/common/serialisation/serialisation.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/rudp/contact.h"

#include "maidsafe/routing/messages/messages_fwd.h"
#include "maidsafe/routing/tests/utils/test_utils.h"

#include "maidsafe/routing/tests/generate_message_header.h"

namespace maidsafe {

namespace routing {

namespace test {

namespace {

ForwardPutData GenerateInstance() {
  const uint32_t max {RandomUint32() % 10 + 1};
  const auto serialised_message {RandomString(Address::kSize)};
  const auto keys {asymm::GenerateKeyPair()};

  std::vector<crypto::SHA1Hash> part;
  part.reserve(max);
  for(uint32_t i {}; i < max; ++i) {
    part.emplace_back(RandomString(crypto::SHA1::DIGESTSIZE));
  }

  return {
    SerialisedData(serialised_message.begin(), serialised_message.end()),
    std::move(part),
    std::move(keys.public_key)
  };
}

}  // anonymous namespace

TEST(ForwardPutDataTest, BEH_SerialiseParse) {
  // Serialise
  ForwardPutData fwd_put_data_before {GenerateInstance()};
  auto tag_before = GivenTypeFindTag_v<ForwardPutData>::value;

  auto serialised_fwd_put_data = Serialise(tag_before, fwd_put_data_before);

  // Parse
  ForwardPutData put_data_after {GenerateInstance()};
  auto tag_after = MessageTypeTag {};

  InputVectorStream binary_input_stream {serialised_fwd_put_data};

  // Parse Tag
  Parse(binary_input_stream, tag_after);

  EXPECT_EQ(tag_before, tag_after);

  // Parse the rest
  Parse(binary_input_stream, put_data_after);

  EXPECT_EQ(fwd_put_data_before.data.size(), put_data_after.data.size());
  EXPECT_TRUE(std::equal(fwd_put_data_before.data.begin(), fwd_put_data_before.data.end(),
                         fwd_put_data_before.data.begin()));
  EXPECT_EQ(fwd_put_data_before.part.size(), put_data_after.part.size());
  EXPECT_TRUE(std::equal(fwd_put_data_before.part.begin(), fwd_put_data_before.part.end(),
                         fwd_put_data_before.part.begin()));
  EXPECT_EQ(fwd_put_data_before.requester_public_key, put_data_after.requester_public_key);
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
