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

using SentinelReturns = std::vector<boost::optional<Sentinel::ResultType>>;

// structure to track messages sent to sentinel and what sentinel result should be
class SignatureGroup {
 public:
  SignatureGroup(GroupAddress group_address, size_t group_size, size_t active_quorum,
                 Authority authority)
      : group_address_(std::move(group_address)),
        group_size_(std::move(group_size)),
        active_quorum_(std::move(active_quorum)),
        authority_(std::move(authority)){
    for (size_t i(0); i < group_size; i++) {
      nodes_.push_back(passport::CreatePmidAndSigner().first);
    }
  }

  GroupAddress SignatureGroupAddress() { return group_address_; }

  std::map<Address, asymm::PublicKey> GetPublicKeys() {
      std::map<Address, asymm::PublicKey> result;
      for (auto node : nodes_)
          result.insert(std::make_pair(Address(node.name()), asymm::PublicKey(node.public_key())));
      return result;
  }

  std::vector<MessageHeader> GetHeaders(DestinationAddress destination_address,
                                        MessageId message_id,
                                        SerialisedData message);

  void SaveSentinelMessageReturn(
          boost::optional<Sentinel::ResultType> sentinel_result) {
      sentinel_message_returns_.push_back(sentinel_result);
  }

  void SaveSentinelSendGetClientKeyReturn(
          boost::optional<Sentinel::ResultType> sentinel_result) {
      sentinel_send_get_client_key_returns_.push_back(sentinel_result);
  }

  void SaveSentinelSendGetGroupKeyReturn(
          boost::optional<Sentinel::ResultType> sentinel_result) {
      sentinel_send_get_group_key_returns_.push_back(sentinel_result);
  }

  SentinelReturns GetAllSentinelReturns() {
      SentinelReturns sentinel_returns_;
      for ( auto sentinel_return : sentinel_message_returns_ ) sentinel_returns_.push_back(sentinel_return);
      for ( auto sentinel_return : sentinel_send_get_client_key_returns_ ) sentinel_returns_.push_back(sentinel_return);
      for ( auto sentinel_return : sentinel_send_get_group_key_returns_ ) sentinel_returns_.push_back(sentinel_return);
      return sentinel_returns_;
  }

  SentinelReturns GetMessageSentinelReturns() {
    return sentinel_message_returns_;
  }


  SentinelReturns GetSendGetGroupKeySentinelReturns() {
    return sentinel_send_get_group_key_returns_;
  }

  SentinelReturns GetSendGetClientKeySentinelReturns() {
    return sentinel_send_get_client_key_returns_;
  }

 private:
  GroupAddress group_address_;
  size_t group_size_;
  size_t active_quorum_;
  Authority authority_;
  std::vector<passport::Pmid> nodes_;

  SentinelReturns sentinel_message_returns_;
  SentinelReturns sentinel_send_get_client_key_returns_;
  SentinelReturns sentinel_send_get_group_key_returns_;
};

std::vector<MessageHeader> SignatureGroup::GetHeaders(DestinationAddress destination_address,
                                                      MessageId message_id,
                                                      SerialisedData message) {
  std::vector<MessageHeader> headers;
  for (const auto node : nodes_) {
    NodeAddress node_address(node.name());
    headers.push_back(
        MessageHeader(destination_address,
                      SourceAddress(node_address, group_address_, boost::none),
                      message_id, authority_, asymm::Sign(asymm::PlainText(message), node.private_key())));
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
                                      boost::none)) {}

  // Add a correct, cooperative group of indictated total size and responsive quorum
  void AddCorrectGroup(GroupAddress group_address, size_t group_size,
                       size_t active_quorum, Authority authority);

  void SimulateMessage(GroupAddress group_address,
                       MessageId message_id,
                       MessageTypeTag tag, SerialisedData message);
  SentinelReturns GetAllSentinelReturns(GroupAddress group_address);
  SentinelReturns GetMessageSentinelReturns(GroupAddress group_address);
  SentinelReturns GetSendGetGroupKeySentinelReturns(GroupAddress group_address);
  SentinelReturns GetSendGetClientKeySentinelReturns(GroupAddress group_address);

  DestinationAddress GetOurDestinationAddress() { return our_destination_; }

  size_t CountSendGetGroupKeyCalls(GroupAddress group_address) {
    return std::count(send_get_group_key_calls_.begin(),
                      send_get_group_key_calls_.end(), group_address);
  }

  size_t CountSendGetClientKeyCalls(GroupAddress group_address) {
    return std::count(send_get_client_key_calls_.begin(),
                      send_get_client_key_calls_.end(), group_address);
  }

  void SendGetClientKey(const Address node_address);
  void SendGetGroupKey(const GroupAddress group_address);

 protected:
  Sentinel sentinel_;
  mutable std::mutex sentinel_mutex_;

 private:
  std::vector<SignatureGroup> groups_;
  passport::Pmid our_pmid_;
  DestinationAddress our_destination_;
  std::vector<GroupAddress> send_get_group_key_calls_;
  std::vector<GroupAddress> send_get_client_key_calls_;
};

void SentinelFunctionalTest::AddCorrectGroup(GroupAddress group_address,
                                             size_t group_size, size_t active_quorum,
                                             Authority authority) {
  auto itr = std::find_if(std::begin(groups_), std::end(groups_),
                          [group_address](SignatureGroup group_)
                          { return group_.SignatureGroupAddress() == group_address; });
  if (itr == std::end(groups_))
      groups_.push_back(SignatureGroup(group_address, group_size, active_quorum, authority));
  else assert("Sentinel test AddCorrectGroup: Group already exists");

  static_cast<void>(itr);
  static_cast<void>(group_address);
  static_cast<void>(group_size);
  static_cast<void>(active_quorum);
}

void SentinelFunctionalTest::SimulateMessage(GroupAddress group_address,
                                             MessageId message_id,
                                             MessageTypeTag tag, SerialisedData message) {
  auto itr = std::find_if(std::begin(groups_), std::end(groups_),
                          [group_address](SignatureGroup group_)
                          { return group_.SignatureGroupAddress() == group_address; });
  if (itr == std::end(groups_)) assert("Sentinel test SimulateMessage: debug Error in test");
  else {
    auto headers = itr->GetHeaders(our_destination_,
                                   message_id, message);
    for (auto header : headers) {
      itr->SaveSentinelMessageReturn(sentinel_.Add(header, tag, message));
    }
  }
}

SentinelReturns SentinelFunctionalTest::GetAllSentinelReturns(GroupAddress group_address) {
  auto itr = std::find_if(std::begin(groups_), std::end(groups_),
                          [group_address](SignatureGroup group_)
                          { return group_.SignatureGroupAddress() == group_address; });
  if (itr == std::end(groups_)) {
    assert("Sentinel test GetSentinelReturns: debug Error in test");
    return SentinelReturns();
  }
  else {
    return itr->GetAllSentinelReturns();
  }
}


SentinelReturns SentinelFunctionalTest::GetMessageSentinelReturns(GroupAddress group_address) {
  auto itr = std::find_if(std::begin(groups_), std::end(groups_),
                          [group_address](SignatureGroup group_)
                          { return group_.SignatureGroupAddress() == group_address; });
  if (itr == std::end(groups_)) {
    assert("Sentinel test GetSentinelReturns: debug Error in test");
    return SentinelReturns();
  }
  else {
    return itr->GetMessageSentinelReturns();
  }
}

SentinelReturns SentinelFunctionalTest::GetSendGetGroupKeySentinelReturns(GroupAddress group_address) {
  auto itr = std::find_if(std::begin(groups_), std::end(groups_),
                          [group_address](SignatureGroup group_)
                          { return group_.SignatureGroupAddress() == group_address; });
  if (itr == std::end(groups_)) {
    assert("Sentinel test GetSentinelReturns: debug Error in test");
    return SentinelReturns();
  }
  else {
    return itr->GetSendGetGroupKeySentinelReturns();
  }
}

SentinelReturns SentinelFunctionalTest::GetSendGetClientKeySentinelReturns(GroupAddress group_address) {
  auto itr = std::find_if(std::begin(groups_), std::end(groups_),
                          [group_address](SignatureGroup group_)
                          { return group_.SignatureGroupAddress() == group_address; });
  if (itr == std::end(groups_)) {
    assert("Sentinel test GetSentinelReturns: debug Error in test");
    return SentinelReturns();
  }
  else {
    return itr->GetSendGetClientKeySentinelReturns();
  }
}

void SentinelFunctionalTest::SendGetClientKey(Address /*node_address*/) {

}

void SentinelFunctionalTest::SendGetGroupKey(GroupAddress group_address) {
  //std::lock_guard<std::mutex> lock(mutex_);
  send_get_group_key_calls_.push_back(group_address);
  auto itr = std::find_if(std::begin(groups_), std::end(groups_),
                          [group_address](SignatureGroup group_)
                          { return group_.SignatureGroupAddress() == group_address; });

  if (itr == std::end(groups_)) assert("Sentinel test SendGetGroupKey: debug Error in test");
  else {
    auto message(Serialise(GetGroupKeyResponse(itr->GetPublicKeys(),
                                               itr->SignatureGroupAddress())));
    auto headers = itr->GetHeaders(our_destination_,
                                   MessageId(RandomUint32()),
                                   message);

    for (auto header : headers) {
        itr->SaveSentinelSendGetGroupKeyReturn(
                    sentinel_.Add(header, MessageTypeTag::GetGroupKeyResponse,
                                  message));
    }
  }
  static_cast<void>(itr);
}

//++++ Free Test Functions +++++++++++++++++++++

size_t CountAllSentinelReturns(const SentinelReturns sentinel_returns) {
  return sentinel_returns.size();
}

size_t CountNoneSentinelReturns(const SentinelReturns sentinel_returns) {
  size_t i(0);
  for ( auto sentinel_return : sentinel_returns )
    if (sentinel_return == boost::none) i++;
  return i;
}

bool VerifyExactlyOneResponse(const SentinelReturns sentinel_returns) {
  size_t counter_responses(0);
  for ( auto sentinel_return : sentinel_returns )
    if (sentinel_return != boost::none) counter_responses++;
  std::cout << "VERIFY EXACTLY ONE: counted " << counter_responses << std::endl;
  return counter_responses == 1;
}

boost::optional<Sentinel::ResultType>
    GetSingleSentinelReturn(const SentinelReturns sentinel_returns) {
  boost::optional<Sentinel::ResultType> sentinel_single_response(boost::none);
  for ( auto sentinel_return : sentinel_returns ) {
    if ( sentinel_return != boost::none ) {
      if ( sentinel_single_response == boost::none )
        sentinel_single_response = sentinel_return;
      else return boost::none;  // double non-none found, return none
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

  std::cout << "SentinelFunctionalTest Verify SentinelReturn Matched! " << std::endl;
  return true;
}


// first try for specific message type, generalise later
// PutData is chosen as fundamental type with data payload.
TEST_F(SentinelFunctionalTest, BEH_SentinelSimpleAdd) {

  const ImmutableData data(NonEmptyString(RandomBytes(1000)));
  PutData put_message(data.TypeId(), Serialise(data));
  SerialisedData serialised_message(Serialise(put_message));

  // Full group will respond correctly to Sentinel requests
  const GroupAddress group_address(MakeIdentity());
  const MessageId message_id(RandomUint32());
  std::cout << "BEH_SentinelSimpleAdd: Original MessageId " << message_id << std::endl;

  AddCorrectGroup(group_address, GroupSize, GroupSize, Authority::client_manager);
  SimulateMessage(group_address, message_id,
                  MessageTypeTag::PutData, serialised_message);
  auto all_returns(GetAllSentinelReturns(group_address));
  auto all_send_get_group_key_returns(GetSendGetGroupKeySentinelReturns(group_address));
  auto all_send_get_client_key_returns(GetSendGetClientKeySentinelReturns(group_address));
  auto all_message_returns(GetMessageSentinelReturns(group_address));

  EXPECT_EQ(2 * GroupSize, CountAllSentinelReturns(all_returns));
  EXPECT_EQ(2 * GroupSize -1, CountNoneSentinelReturns(all_returns));
  EXPECT_EQ(0, CountAllSentinelReturns(all_send_get_client_key_returns));
  EXPECT_EQ(GroupSize, CountAllSentinelReturns(all_send_get_group_key_returns));
  EXPECT_EQ(GroupSize, CountAllSentinelReturns(all_message_returns));
  EXPECT_TRUE(VerifyExactlyOneResponse(all_returns));

  EXPECT_EQ(1, CountSendGetGroupKeyCalls(group_address));
  EXPECT_EQ(0, CountSendGetClientKeyCalls(group_address));

  EXPECT_TRUE(VerifyMatchSentinelReturn(GetSingleSentinelReturn(all_returns),
                                        message_id,
                                        Authority::client_manager,
                                        GetOurDestinationAddress(),
                                        group_address,
                                        MessageTypeTag::PutData,
                                        serialised_message));
}



/* //Simply to time surrounding execution time,
 * //roughly 110ms for 69 messages handled in Sentinel for 1 message
 *
TEST_F(SentinelFunctionalTest, BEH_QuickTimerTest) {

  const ImmutableData data(NonEmptyString(RandomBytes(100)));
  PutData put_message(data.TypeId(), Serialise(data));

  // Full group will respond correctly to Sentinel requests
  const GroupAddress group_address(MakeIdentity());
  AddCorrectGroup(group_address, GroupSize, GroupSize, Authority::client_manager);
  //SimulateMessage(group_address,
  //                MessageTypeTag::PutData, Serialise(put_message));
  auto returns(GetAllSentinelReturns(group_address));
}
*/

}  // namespacce test

}  // namespace routing

}  // namespace maidsafe

