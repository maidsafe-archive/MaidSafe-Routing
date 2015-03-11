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
  SentinelTest() : sentinel_(new Sentinel([](Address) {}, [](GroupAddress) {})),
                   maid_nodes_(),
                   source_address_(NodeAddress(NodeId(RandomString(NodeId::kSize))), boost::none,
                                               boost::none) {}
  
  struct SentinelAddInfo {
    MessageHeader header;
    MessageTypeTag tag;
    SerialisedMessage serialised;
  };

  template <typename MessageType>
  SentinelAddInfo MakeAddInfo(const MessageType& message, const asymm::PrivateKey& private_key,
                              const DestinationAddress& destination, const SourceAddress& source,
                              MessageId message_id, Authority our_authority, MessageTypeTag tag) {
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

  void CreatePmidKeys(size_t quantity) {
    while (quantity-- > 0)
      pmid_nodes_.emplace_back(passport::CreatePmidAndSigner().first);
  }
  
  SentinelAddInfo CreateGetKeyResponse(const passport::Maid& maid, MessageId message_id,
                                       Address source, passport::Pmid& pmid) {
    GetClientKeyResponse get_client_response(Address(maid.name()->string()), maid.public_key());
    return MakeAddInfo(get_client_response,
                       pmid.private_key(),
                       DestinationAddress(std::make_pair(Destination(source), boost::none)),
                       SourceAddress(NodeAddress(NodeId(pmid.name()->string())),
                                     GroupAddress(source), boost::none),
                       message_id, Authority::nae_manager,
                       MessageTypeTag::GetClientKeyResponse);
  }

  std::vector<SentinelAddInfo> CreateGetGroupKeyResponse(MessageId message_id,
                                                         const GroupAddress& target,
                                                         const Address& source) {
    assert(pmid_nodes_.size() >= GroupSize);
    std::vector<SentinelAddInfo> group_key_response;
    std::sort(pmid_nodes_.begin(), pmid_nodes_.end(),
              [&](const passport::Pmid& lhs, const passport::Pmid& rhs) {
                return NodeId::CloserToTarget(NodeId(lhs.name()->string()),
                                              NodeId(rhs.name()->string()),
                                              target.data);
              });
    std::map<Address, asymm::PublicKey> public_key_map;
    for (size_t index(0); index < GroupSize; ++index)
      public_key_map.insert(std::make_pair(Address(pmid_nodes_.at(index).name()->string()),
                                           pmid_nodes_.at(index).public_key()));
    GetGroupKeyResponse get_group_key_response(public_key_map, target);
    for (size_t index(0); index < GroupSize; ++index) {
      group_key_response.push_back(
          MakeAddInfo(get_group_key_response,
                      pmid_nodes_.at(index).private_key(),
                      DestinationAddress(std::make_pair(Destination(source), boost::none)),
                      SourceAddress(NodeAddress(NodeId(pmid_nodes_.at(index).name()->string())),
                                    target, boost::none),
                      message_id, Authority::nae_manager, MessageTypeTag::GetGroupKeyResponse));
    }
    assert(group_key_response.size() >= GroupSize);
    return group_key_response;
  }

  template<typename MessageType>
  std::vector<SentinelAddInfo>
  CreateGroupMessage(const MessageType& message, MessageId message_id, Authority authority,
                     MessageTypeTag message_type, const GroupAddress& target,
                     const GroupAddress& source) {
    assert(pmid_nodes_.size() >= GroupSize);
    std::vector<SentinelAddInfo> group_message;
    std::sort(pmid_nodes_.begin(), pmid_nodes_.end(),
              [&](const passport::Pmid& lhs, const passport::Pmid& rhs) {
                return NodeId::CloserToTarget(NodeId(rhs.name()->string()),
                                              NodeId(lhs.name()->string()),
                                              target.data);
              });

    for (size_t index(0); index < GroupSize; ++index) {
      group_message.emplace_back(
          MakeAddInfo(message, pmid_nodes_.at(index).private_key(),
                      DestinationAddress(std::make_pair(Destination(target.data), boost::none)),
                      SourceAddress(NodeAddress(NodeId(pmid_nodes_.at(index).name()->string())),
                                    source, boost::none),
                      message_id, authority, message_type));
    }

    assert(group_message.size() >= GroupSize);
    return group_message;
  }

  
 protected:
  std::unique_ptr<Sentinel> sentinel_;
  std::vector<passport::Maid> maid_nodes_;
  std::vector<passport::Pmid> pmid_nodes_;
  SourceAddress source_address_;
};

TEST_F(SentinelTest, BEH_BasicNonGroupAdd) {
  CreateMaidKeys(1);
  CreatePmidKeys(QuorumSize);
  ImmutableData data(NonEmptyString(RandomString(NodeId::kSize)));
  auto serialised_data_string(data.Serialise().data.string());
  PutData put_data(DataTagValue::kImmutableDataValue, SerialisedData(serialised_data_string.begin(),
                                                                     serialised_data_string.end()));
  auto add_info(MakeAddInfo(put_data, maid_nodes_.at(0).private_key(),
                            DestinationAddress(
                                std::make_pair(Destination(
                                                   NodeId(maid_nodes_.at(0).name()->string())),
                                               boost::none)),
                            source_address_,
                            MessageId(RandomUint32()), Authority::client_manager,
                            MessageTypeTag::PutData));

  auto resolved(this->sentinel_->Add(add_info.header, add_info.tag, add_info.serialised));
  if (resolved)
    EXPECT_TRUE(false);

  for (size_t index(0); index < QuorumSize - 1; ++index) {
      auto add_key_info(CreateGetKeyResponse(maid_nodes_.at(0), add_info.header.MessageId(),
                                             source_address_.node_address.data,
                                             pmid_nodes_.at(index)));
    resolved = (this->sentinel_->Add(add_key_info.header, add_key_info.tag,
                                     add_key_info.serialised));
    if (resolved)
      EXPECT_TRUE(false);
  }
  auto add_key_info(CreateGetKeyResponse(maid_nodes_.at(0), add_info.header.MessageId(),
                                         source_address_.node_address.data,
                                         pmid_nodes_.at(QuorumSize - 1)));
  resolved = (this->sentinel_->Add(add_key_info.header, add_key_info.tag,
                                   add_key_info.serialised));
  if (!resolved)
    EXPECT_TRUE(false);
}

TEST_F(SentinelTest, BEH_BasicGroupAdd) {
  CreatePmidKeys(GroupSize * 4);
  ImmutableData data(NonEmptyString(RandomString(NodeId::kSize)));
  auto serialised_data_string(data.Serialise().data.string());
  PutData put_data(DataTagValue::kImmutableDataValue, SerialisedData(serialised_data_string.begin(),
                                                                     serialised_data_string.end()));
  MessageId message_id(RandomInt32());
  auto group_message(CreateGroupMessage(put_data, message_id, Authority::node_manager,
                                        MessageTypeTag::PutData,
                                        GroupAddress(NodeId(data.name()->string())),
                                        GroupAddress(source_address_.node_address.data)));

  auto group_key_response(
      CreateGetGroupKeyResponse(message_id, GroupAddress(NodeId(data.name()->string())),
                                GroupAddress(source_address_.node_address.data)));
  for (const auto& add_key_info : group_message) {
    auto resolved(this->sentinel_->Add(add_key_info.header, add_key_info.tag,
                                       add_key_info.serialised));
    if (resolved)
      EXPECT_TRUE(false);
  }
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
