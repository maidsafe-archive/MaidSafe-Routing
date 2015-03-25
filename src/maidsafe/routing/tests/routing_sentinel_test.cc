/*  Copyright 2015 MaidSafe.net limited

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


#include <chrono>
#include <thread>
#include <utility>

#include "maidsafe/routing/sentinel.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/identity.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/common/data_types/immutable_data.h"

#include "maidsafe/routing/types.h"
#include "maidsafe/routing/message_header.h"
#include "maidsafe/routing/messages/messages.h"
#include "maidsafe/routing/messages/messages_fwd.h"


namespace maidsafe {

namespace routing {

namespace test {

using SentinelMessageTrack = uint32_t;
using SentinelReturns = std::map<SentinelMessageTrack,
                                 boost::optional<Sentinel::ResultType>>;
using SentinelAddMessage = std::tuple<MessageHeader, MessageTypeTag, SerialisedMessage,
                                      SentinelMessageTrack>;
using SentinelCountQuorum = std::tuple<size_t, size_t, bool>;

// structure to track messages sent to sentinel and what sentinel result should be
class SignatureGroup {
 public:
  SignatureGroup(GroupAddress group_address, size_t group_size,
                 Authority authority)
      : group_address_(std::move(group_address)),
        group_size_(std::move(group_size)),
        authority_(std::move(authority)){
    for (size_t i(0); i < group_size; i++) {
      nodes_.push_back(passport::CreatePmidAndSigner().first);
    }
  }

  GroupAddress SignatureGroupAddress() { return group_address_; }

  std::map<Address, asymm::PublicKey> GetPublicKeys() {
      std::map<Address, asymm::PublicKey> result;
      for (auto node : nodes_) {
          result.insert(std::make_pair(Address(node.name()), node.public_key()));
      }
      return result;
  }

  std::vector<MessageHeader> GetHeaders(DestinationAddress destination_address,
                                        MessageId message_id,
                                        SerialisedData message);

 private:
  GroupAddress group_address_;
  size_t group_size_;
  Authority authority_;
  std::vector<passport::Pmid> nodes_;
};

std::vector<MessageHeader> SignatureGroup::GetHeaders(DestinationAddress destination_address,
                                                      MessageId message_id,
                                                      SerialisedMessage message) {
  std::vector<MessageHeader> headers;
  for (const auto node : nodes_) {
    NodeAddress node_address(node.name());
    headers.push_back(
        MessageHeader(destination_address,
                      SourceAddress(node_address, group_address_, boost::none),
                      message_id, authority_, asymm::Sign(asymm::PlainText(message),
                                                          node.private_key())));
  }

  return headers;
}

// persistent sentinel throughout all sentinel tests
class SentinelFunctionalTest : public testing::Test {
 public:
  SentinelFunctionalTest()
    : sentinel_([this](Address address) { SendGetClientKey(address); },
                [this](GroupAddress group_address) { SendGetGroupKey(group_address); }),
      our_pmid_(passport::CreatePmidAndSigner().first),
      our_destination_(std::make_pair(Destination(our_pmid_.name()),
                                      boost::none)),
      send_get_group_key_calls_(),
      send_get_client_key_calls_(),
      message_tracker_(0),
      sentinel_returns_() {}

    std::vector<SentinelAddMessage>
        GenerateMessages(std::vector<MessageHeader> headers,
                                                 MessageTypeTag tag,
                                                 SerialisedMessage message);
  void AddToSentinel(SentinelAddMessage message);
  void AddToSentinel(std::vector<SentinelAddMessage> messages);

  DestinationAddress GetOurDestinationAddress() { return our_destination_; }

  void SaveSentinelReturn(SentinelMessageTrack key,
                          boost::optional<Sentinel::ResultType> sentinel_result) {
    sentinel_returns_.insert(std::make_pair(key, sentinel_result));
  }

  SentinelReturns GetAllSentinelReturns() { return sentinel_returns_; }

  SentinelReturns GetSelectedSentinelReturns(
                          std::vector<SentinelMessageTrack> track_messages);

  size_t CountSendGetGroupKeyCalls(GroupAddress group_address) {
    return std::count(send_get_group_key_calls_.begin(),
                      send_get_group_key_calls_.end(), group_address);
  }

  size_t CountSendGetClientKeyCalls(Address node_address) {
    return std::count(send_get_client_key_calls_.begin(),
                      send_get_client_key_calls_.end(), node_address);
  }

  void SendGetClientKey(const Address node_address);
  void SendGetGroupKey(const GroupAddress group_address);

 protected:
  Sentinel sentinel_;
  mutable std::mutex sentinel_mutex_;

 private:
  passport::Pmid our_pmid_;
  DestinationAddress our_destination_;
  std::vector<GroupAddress> send_get_group_key_calls_;
  std::vector<Address> send_get_client_key_calls_;
  SentinelMessageTrack message_tracker_;

  SentinelReturns sentinel_returns_;
};

std::vector<SentinelAddMessage>
    SentinelFunctionalTest::GenerateMessages(std::vector<MessageHeader> headers,
                                                 MessageTypeTag tag,
                                                 SerialisedMessage message) {
    std::vector<SentinelAddMessage> collect_messages;
    for ( auto header : headers )
      collect_messages.push_back(std::make_tuple(header, tag, message, message_tracker_++));
    return collect_messages;
}


void SentinelFunctionalTest::AddToSentinel(SentinelAddMessage message) {
  auto sentinel_response(sentinel_.Add(std::get<0>(message),  // Message Header
                                        std::get<1>(message),  // Message Type Tag
                                        std::get<2>(message)));  // Serialised Message
  SaveSentinelReturn(std::get<3>(message), sentinel_response);  // Sentinel message tracking key
}


void SentinelFunctionalTest::AddToSentinel(std::vector<SentinelAddMessage> messages) {
  for ( auto message : messages ) {
    auto sentinel_response(sentinel_.Add(std::get<0>(message),  // Message Header
                                          std::get<1>(message),  // Message Type Tag
                                          std::get<2>(message)));  // Serialised Message
    SaveSentinelReturn(std::get<3>(message), sentinel_response);  // Sentinel message tracking key
  }
}

SentinelReturns SentinelFunctionalTest::GetSelectedSentinelReturns(
                                          std::vector<SentinelMessageTrack> track_messages) {
  if (track_messages.empty()) return SentinelReturns();
  SentinelReturns select_sentinel_returns;
  std::sort(track_messages.begin(), track_messages.end(),  // sort in descending order
            std::greater<SentinelMessageTrack>());
  auto itr_track = std::prev(track_messages.end());
  for ( auto sentinel_return : sentinel_returns_ ) {
    if ( sentinel_return.first == *itr_track )
      select_sentinel_returns.insert(sentinel_return);
    if ( sentinel_return.first >= *itr_track ) {
      if ( itr_track != track_messages.begin() ) {
        itr_track = std::prev(itr_track);
        track_messages.pop_back();
      } else { break; }
    }
  }

  return select_sentinel_returns;
}

void SentinelFunctionalTest::SendGetClientKey(Address node_address) {
  send_get_client_key_calls_.push_back(node_address);
}

void SentinelFunctionalTest::SendGetGroupKey(GroupAddress group_address) {
  send_get_group_key_calls_.push_back(group_address);
}

// ++++ Free Test Functions +++++++++++++++++++++

size_t CountAllSentinelReturns(const SentinelReturns sentinel_returns) {
  return sentinel_returns.size();
}

size_t CountNoneSentinelReturns(const SentinelReturns sentinel_returns) {
  size_t i(0);
  for ( auto sentinel_return : sentinel_returns ) {
    if (sentinel_return.second == boost::none) i++;
  }
  return i;
}

bool VerifyExactlyOneResponse(const SentinelReturns sentinel_returns) {
  size_t counter_responses(0);
  for ( auto sentinel_return : sentinel_returns )
    if (sentinel_return.second != boost::none) counter_responses++;
  return counter_responses == 1;
}

boost::optional<Sentinel::ResultType>
    GetSingleSentinelReturn(const SentinelReturns sentinel_returns) {
  boost::optional<Sentinel::ResultType> sentinel_single_response(boost::none);
  for ( auto sentinel_return : sentinel_returns ) {
    if ( sentinel_return.second != boost::none ) {
      if ( sentinel_single_response == boost::none ) {
        sentinel_single_response = sentinel_return.second;
      } else {
        return boost::none;  // double non-none found, return none
      }
    }
  }
  return sentinel_single_response;
}

bool VerifyMatchSentinelReturn(const boost::optional<Sentinel::ResultType> sentinel_return,
                               const MessageId original_message_id,
                               const Authority original_authority,
                               const DestinationAddress original_destination,
                               const GroupAddress original_source_group,
                               const MessageTypeTag original_message_type_tag,
                               const SerialisedMessage original_message) {
  if ( sentinel_return == boost::none ) return false;
  MessageHeader return_message_header(std::get<0>(*sentinel_return));
  MessageTypeTag return_message_tag(std::get<1>(*sentinel_return));
  SerialisedMessage return_message(std::get<2>(*sentinel_return));

  if ( return_message != original_message ) return false;
  if ( return_message_tag != original_message_type_tag ) return false;
  if ( return_message_header.MessageId() != original_message_id ) return false;
  if ( return_message_header.FromAuthority() != original_authority ) return false;
  if ( return_message_header.Destination() != original_destination ) return false;
  if ( *(return_message_header.Source().group_address) != original_source_group ) return false;

  return true;
}

std::vector<SentinelMessageTrack> ExtractMessageTrackers(
        const std::vector<SentinelAddMessage>& messages) {
  std::vector<SentinelMessageTrack> message_trackers;
  for ( auto message : messages ) message_trackers.push_back(std::get<3>(message));
  return message_trackers;
}

std::vector<SentinelMessageTrack> ExtractMessageTracker(
        const SentinelAddMessage& message) {
  std::vector<SentinelMessageTrack> message_trackers;
  message_trackers.push_back(std::get<3>(message));
  return message_trackers;
}

std::vector<SentinelMessageTrack> IdentifyQuorumMessages(
        const std::vector<SentinelAddMessage>& messages) {
  std::vector<SentinelMessageTrack> result_trackers;
  std::map<GroupAddress, SentinelCountQuorum> quorum_counter;
  for ( auto message : messages ) {
    if ( !std::get<0>(message).FromGroup() ) break;
    if ( std::get<1>(message) == MessageTypeTag::GetGroupKeyResponse ) {
      auto itr = quorum_counter.find(*(std::get<0>(message).FromGroup()));
      if ( itr != quorum_counter.end() ) {
        std::get<1>(itr->second)++;
        if ( std::get<0>(itr->second) >= QuorumSize &&
             std::get<1>(itr->second) >= QuorumSize &&
            !std::get<2>(itr->second) ) {
          result_trackers.push_back(std::get<3>(message));
          std::get<2>(itr->second) = true;
        }
      }
    } else {
      auto itr = quorum_counter.find(*(std::get<0>(message).FromGroup()));
      if ( itr != quorum_counter.end() ) {
        std::get<0>(itr->second)++;
        if ( std::get<0>(itr->second) >= QuorumSize &&
             std::get<1>(itr->second) >= QuorumSize &&
            !std::get<2>(itr->second) ) {
          result_trackers.push_back(std::get<3>(message));
          std::get<2>(itr->second) = true;
        }
      } else {
        quorum_counter.insert(std::make_pair(*(std::get<0>(message).FromGroup()),
                                             SentinelCountQuorum(1U, 0U, false)));
      }
    }
  }
  return result_trackers;
}

// +++++++++++++++ Tests +++++++++++++++++++++

// first try for specific message type, generalise later
// PutData is chosen as fundamental type with data payload.
TEST_F(SentinelFunctionalTest, FUNC_SentinelSimpleAddRespondWithNewMessageId) {
  const GroupAddress group_address(MakeIdentity());
  SignatureGroup single_group(group_address, GroupSize, Authority::client_manager);

  // Generate PutData messages
  const ImmutableData data(NonEmptyString(RandomBytes(3)));
  PutData put_data(data.TypeId(), Serialise(data));
  auto serialised_put_data(Serialise(put_data));
  const MessageId message_id_put_data(RandomUint32());
  auto headers_put_data(single_group.GetHeaders(GetOurDestinationAddress(),
                                       message_id_put_data, serialised_put_data));
  auto put_data_messages(GenerateMessages(headers_put_data,
                                          MessageTypeTag::PutData, serialised_put_data));
  auto message_trackers(ExtractMessageTrackers(put_data_messages));

  // Generate GetGroupKeyResponses
  auto serialised_get_group_response(Serialise(GetGroupKeyResponse(
              single_group.GetPublicKeys(), single_group.SignatureGroupAddress())));
  const MessageId message_id_get_group_key_response(RandomUint32());
  auto headers_response(single_group.GetHeaders(GetOurDestinationAddress(),
                                                message_id_get_group_key_response,
                                                serialised_get_group_response));
  auto response_messages(GenerateMessages(headers_response,
                                          MessageTypeTag::GetGroupKeyResponse,
                                          serialised_get_group_response));
  auto response_trackers(ExtractMessageTrackers(response_messages));
  auto expected_valid_response_tracker(
                         ExtractMessageTracker(response_messages.at(QuorumSize - 1)));

  // Send all messages to Sentinel
  AddToSentinel(put_data_messages);
  auto message_returns(GetSelectedSentinelReturns(message_trackers));

  EXPECT_EQ(GroupSize, CountAllSentinelReturns(message_returns));
  EXPECT_EQ(GroupSize, CountNoneSentinelReturns(message_returns));
  EXPECT_FALSE(VerifyExactlyOneResponse(message_returns));
  EXPECT_EQ(1, CountSendGetGroupKeyCalls(single_group.SignatureGroupAddress()));
  EXPECT_EQ(0, CountSendGetClientKeyCalls(single_group.SignatureGroupAddress()));

  // Send all GetGroupKeyResponses to Sentinel
  AddToSentinel(response_messages);
  auto response_returns(GetSelectedSentinelReturns(response_trackers));
  auto expected_valid_response_return(
                        GetSelectedSentinelReturns(expected_valid_response_tracker));

  EXPECT_EQ(GroupSize, CountAllSentinelReturns(response_returns));
  EXPECT_EQ(GroupSize, CountNoneSentinelReturns(response_returns));
  EXPECT_EQ(1, CountNoneSentinelReturns(expected_valid_response_return));
  EXPECT_FALSE(VerifyExactlyOneResponse(response_returns));
  EXPECT_EQ(1, CountSendGetGroupKeyCalls(single_group.SignatureGroupAddress()));
  EXPECT_EQ(0, CountSendGetClientKeyCalls(single_group.SignatureGroupAddress()));

  EXPECT_FALSE(VerifyMatchSentinelReturn(GetSingleSentinelReturn(response_returns),
                                         message_id_put_data,
                                         Authority::client_manager,
                                         GetOurDestinationAddress(),
                                         single_group.SignatureGroupAddress(),
                                         MessageTypeTag::PutData,
                                         serialised_put_data));
}

TEST_F(SentinelFunctionalTest, FUNC_SentinelSimpleAdd) {
  const GroupAddress group_address(MakeIdentity());
  SignatureGroup single_group(group_address, GroupSize, Authority::client_manager);

  // Generate PutData messages
  const ImmutableData data(NonEmptyString(RandomBytes(3)));
  PutData put_data(data.TypeId(), Serialise(data));
  auto serialised_put_data(Serialise(put_data));
  const MessageId message_id_put_data(RandomUint32());
  auto headers_put_data(single_group.GetHeaders(GetOurDestinationAddress(),
                                       message_id_put_data, serialised_put_data));
  auto put_data_messages(GenerateMessages(headers_put_data,
                                          MessageTypeTag::PutData, serialised_put_data));
  auto message_trackers(ExtractMessageTrackers(put_data_messages));

  // Generate GetGroupKeyResponses
  auto serialised_get_group_response(Serialise(GetGroupKeyResponse(
              single_group.GetPublicKeys(), single_group.SignatureGroupAddress())));
  auto headers_response(single_group.GetHeaders(GetOurDestinationAddress(),
                                                message_id_put_data,
                                                serialised_get_group_response));
  auto response_messages(GenerateMessages(headers_response,
                                          MessageTypeTag::GetGroupKeyResponse,
                                          serialised_get_group_response));
  auto response_trackers(ExtractMessageTrackers(response_messages));
  auto expected_valid_response_tracker(
                         ExtractMessageTracker(response_messages.at(QuorumSize - 1)));

  // Send all messages to Sentinel
  AddToSentinel(put_data_messages);
  auto message_returns(GetSelectedSentinelReturns(message_trackers));

  EXPECT_EQ(GroupSize, CountAllSentinelReturns(message_returns));
  EXPECT_EQ(GroupSize, CountNoneSentinelReturns(message_returns));
  EXPECT_FALSE(VerifyExactlyOneResponse(message_returns));
  EXPECT_EQ(1, CountSendGetGroupKeyCalls(single_group.SignatureGroupAddress()));
  EXPECT_EQ(0, CountSendGetClientKeyCalls(single_group.SignatureGroupAddress()));

  // Send all GetGroupKeyResponses to Sentinel
  AddToSentinel(response_messages);
  auto response_returns(GetSelectedSentinelReturns(response_trackers));
  auto expected_valid_response_return(
                        GetSelectedSentinelReturns(expected_valid_response_tracker));

  std::vector<SentinelAddMessage> messages;
  for ( auto message : put_data_messages )
      messages.push_back(message);  // add response messages
  for ( auto message : response_messages )
      messages.push_back(message);  // add response messages
  EXPECT_EQ(expected_valid_response_tracker,
            IdentifyQuorumMessages(messages));

  EXPECT_EQ(GroupSize, CountAllSentinelReturns(response_returns));
  EXPECT_EQ(GroupSize - 1, CountNoneSentinelReturns(response_returns));
  EXPECT_EQ(0, CountNoneSentinelReturns(expected_valid_response_return));
  EXPECT_TRUE(VerifyExactlyOneResponse(response_returns));
  EXPECT_EQ(1, CountSendGetGroupKeyCalls(single_group.SignatureGroupAddress()));
  EXPECT_EQ(0, CountSendGetClientKeyCalls(single_group.SignatureGroupAddress()));

  EXPECT_TRUE(VerifyMatchSentinelReturn(GetSingleSentinelReturn(response_returns),
                                        message_id_put_data,
                                        Authority::client_manager,
                                        GetOurDestinationAddress(),
                                        single_group.SignatureGroupAddress(),
                                        MessageTypeTag::PutData,
                                        serialised_put_data));
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe

