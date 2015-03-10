/*  Copyright 2012 MaidSafe.net limited

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

#include <memory>
#include <vector>

#include "maidsafe/common/rsa.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/common/make_unique.h"

#include "maidsafe/passport/types.h"

#include "maidsafe/routing/sentinel.h"
#include "maidsafe/routing/messages/messages.h"

namespace maidsafe {

namespace routing {

namespace test {

class SentinelTest : public testing::Test {
 public:
  SentinelTest() : sentinel_(new Sentinel([](Address) {}, [](GroupAddress) {})) {}
  
  struct SentinelAddInfo {
    MessageHeader header;
    MessageTypeTag tag;
    SerialisedMessage serialised;
  };

  template <typename MessageType>
  SentinelAddInfo MakeAddInfo(const MessageType& message, const asymm::PrivateKey& private_key,
                              DestinationAddress destination, SourceAddress source,
                              MessageId message_id, Authority our_authority,
                              MessageTypeTag tag) {
    SentinelAddInfo add_info;
    add_info.tag = tag;
    add_info.serialised = Serialise(message);
    add_info.header = MessageHeader(destination, source, message_id, our_authority,
                                    asymm::Sign(add_info.serialised, private_key));
    return add_info;
  }
  
  void CreateMaidKeys(size_t quantity) {
    while (quantity-- > 0)
      maid_nodes_.emplace_back(passport::CreateMaidAndSigner().first);
  }
  
  SentinelAddInfo CreateGerKeyResponse(const passport::Maid& maid, MessageId message_id) {
    
    return MakeAddInfo(<#const MessageType &message#>, <#const asymm::PrivateKey &private_key#>, <#DestinationAddress destination#>, <#maidsafe::routing::SourceAddress source#>, <#MessageId message_id#>, <#maidsafe::routing::Authority our_authority#>, <#maidsafe::routing::MessageTypeTag tag#>)
  }
  
 protected:
  std::unique_ptr<Sentinel> sentinel_;
  std::vector<passport::Maid> maid_nodes_;
};

TEST_F(SentinelTest, BEH_BasicNonGroupAdd) {
  CreateMaidKeys(1);
  ImmutableData data(NonEmptyString(RandomString(NodeId::kSize)));
  auto serialised_data_string(data.Serialise().data.string());
  PutData put_data(DataTagValue::kImmutableDataValue, SerialisedData(serialised_data_string.begin(),
                                                                     serialised_data_string.end()));
  auto add_info(MakeAddInfo(put_data, maid_nodes_.at(0).private_key(),
                            DestinationAddress(
                                std::make_pair(Destination(NodeId(RandomString(NodeId::kSize))),
                                               boost::none)),
                            SourceAddress(NodeAddress(NodeId(maid_nodes_.at(0).name()->string())),
                                          boost::none, boost::none),
                            MessageId(RandomUint32()), Authority::client_manager,
                            MessageTypeTag::PutData));

  auto resolved(this->sentinel_->Add(add_info.header, add_info.tag, add_info.serialised));
  if (resolved)
    EXPECT_TRUE(false);
  
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
