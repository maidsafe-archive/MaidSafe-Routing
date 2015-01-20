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

#include "maidsafe/routing/messages/put_data_response.h"

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

PutDataResponse GenerateInstance() {
  return {Address{RandomString(Address::kSize)}, MakeError(CommonErrors::unknown)};
}

TEST(PutDataResponseTest, BEH_SerialiseParse) {
  // Serialise
  auto put_data_rsp_before(GenerateInstance());
  auto header_before(GenerateMessageHeader());
  auto tag_before(MessageToTag<PutDataResponse>::value());

  auto serialised_put_data_rsp(Serialise(header_before, tag_before, put_data_rsp_before));

  // Parse
  auto put_data_rsp_after(GenerateInstance());
  auto header_after(GenerateMessageHeader());
  auto tag_after(MessageTypeTag{});

  InputVectorStream binary_input_stream{serialised_put_data_rsp};

  // Parse Header, Tag
  Parse(binary_input_stream, header_after, tag_after);

  EXPECT_EQ(header_before, header_after);
  EXPECT_EQ(tag_before, tag_after);

  // Parse the rest
  Parse(binary_input_stream, put_data_rsp_after);

  EXPECT_EQ(put_data_rsp_before.key, put_data_rsp_after.key);


  EXPECT_EQ(ErrorToInt(put_data_rsp_before.result), ErrorToInt(put_data_rsp_after.result));
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
