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

#include "maidsafe/routing/messages/client_post.h"

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

ClientPost GenerateInstance() {
  const auto serialised_message(RandomString(Address::kSize));

  return {SerialisedData(serialised_message.begin(), serialised_message.end()),
          crypto::SHA1Hash(RandomString(CryptoPP::SHA1::DIGESTSIZE))};
}

}  // anonymous namespace

TEST(ClientPostTest, BEH_SerialiseParse) {
  // Serialise
  auto fwd_post_before(GenerateInstance());
  auto header_before(GenerateMessageHeader());
  auto tag_before(GivenTypeFindTag_v<ClientPost>::value);

  auto serialised_fwd_post(Serialise(header_before, tag_before, fwd_post_before));

  // Parse
  auto fwd_post_after(GenerateInstance());
  auto header_after(GenerateMessageHeader());
  auto tag_after(MessageTypeTag{});

  InputVectorStream binary_input_stream{serialised_fwd_post};

  // Parse Header, Tag
  Parse(binary_input_stream, header_after, tag_after);

  EXPECT_EQ(header_before, header_after);
  EXPECT_EQ(tag_before, tag_after);

  // Parse the rest
  Parse(binary_input_stream, fwd_post_after);

  EXPECT_EQ(fwd_post_before.data.size(), fwd_post_after.data.size());
  EXPECT_TRUE(std::equal(fwd_post_before.data.begin(), fwd_post_before.data.end(),
                         fwd_post_after.data.begin()));

  EXPECT_EQ(fwd_post_before.part.size(), fwd_post_after.part.size());
  EXPECT_TRUE(std::equal(fwd_post_before.part.begin(), fwd_post_before.part.end(),
                         fwd_post_after.part.begin()));
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
