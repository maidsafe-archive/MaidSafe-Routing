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

PutDataResponse GenerateInstance() {
  const auto destination_id = DestinationAddress{Address{RandomString(Address::kSize)}};
  const auto source_id = SourceAddress{Address{RandomString(Address::kSize)}};
  const auto address {Address{RandomString(Address::kSize)}};

  const auto serialised_data {RandomString(Address::kSize)};

  return PutDataResponse {
    PutData {
      destination_id,
          source_id,
          address,
          asymm::Signature(),
          SerialisedData(serialised_data.begin(), serialised_data.end()),
          3
    },
    MakeError(CommonErrors::unknown)
  };
}

TEST(PutDataResponseTest, BEH_SerialiseParse) {
  // Serialise
  PutDataResponse put_data_rsp_before {GenerateInstance()};
  auto tag_before = GivenTypeFindTag_v<PutDataResponse>::value;

  auto serialised_put_data_rsp = Serialise(tag_before, put_data_rsp_before);

  // Parse
  PutDataResponse put_data_rsp_after {GenerateInstance()};
  auto tag_after = MessageTypeTag{};

  InputVectorStream binary_input_stream{serialised_put_data_rsp};

  // Parse Tag
  Parse(binary_input_stream, tag_after);

  EXPECT_EQ(tag_before, tag_after);

  // Parse the rest
  Parse(binary_input_stream, put_data_rsp_after);

  EXPECT_EQ(put_data_rsp_before.header, put_data_rsp_after.header);
  EXPECT_EQ(put_data_rsp_before.data_name, put_data_rsp_after.data_name);
  EXPECT_EQ(put_data_rsp_before.result, put_data_rsp_after.result);
  EXPECT_EQ(put_data_rsp_before.part, put_data_rsp_after.part);
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
