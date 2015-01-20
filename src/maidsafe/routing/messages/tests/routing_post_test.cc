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

#include "maidsafe/routing/messages/post.h"

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

Post GenerateInstance() {
  const auto serialised_data(RandomString(Address::kSize));

  return {Address{RandomString(Address::kSize)},
          SerialisedData(serialised_data.begin(), serialised_data.end()),
          crypto::SHA1Hash(RandomString(CryptoPP::SHA1::DIGESTSIZE))};
}

}  // anonymous namespace

TEST(PostTest, BEH_SerialiseParse) {
  // Serialise
  auto post_before(GenerateInstance());
  auto header_before(GenerateMessageHeader());
  auto tag_before(MessageToTag<Post>::value());

  auto serialised_post(Serialise(header_before, tag_before, post_before));

  // Parse
  auto post_after(GenerateInstance());
  auto header_after(GenerateMessageHeader());
  auto tag_after(MessageTypeTag{});

  InputVectorStream binary_input_stream{serialised_post};

  // Parse Header, Tag
  Parse(binary_input_stream, header_after, tag_after);

  EXPECT_EQ(header_before, header_after);
  EXPECT_EQ(tag_before, tag_after);

  // Parse the rest
  Parse(binary_input_stream, post_after);

  EXPECT_EQ(post_before.key, post_after.key);

  EXPECT_EQ(post_before.data.size(), post_after.data.size());
  EXPECT_TRUE(
      std::equal(post_before.data.begin(), post_before.data.end(), post_before.data.begin()));

  EXPECT_EQ(post_before.part.size(), post_after.part.size());
  EXPECT_TRUE(
      std::equal(post_before.part.begin(), post_before.part.end(), post_before.part.begin()));
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
