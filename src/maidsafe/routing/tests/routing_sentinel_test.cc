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

  void SaveSentinelReturn(
          boost::optional<Sentinel::ResultType> sentinel_result) {
      sentinel_returns_.push_back(sentinel_result);
  }

  SentinelReturns GetSentinelReturns() {
      return sentinel_returns_;
  }

 private:
  GroupAddress group_address_;
  size_t group_size_;
  size_t active_quorum_;
  Authority authority_;
  std::vector<passport::Pmid> nodes_;

  SentinelReturns sentinel_returns_;
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
                       MessageTypeTag tag, SerialisedData message);
  SentinelReturns GetSentinelReturns(GroupAddress group_address);

  void SendGetClientKey(const Address node_address);
  void SendGetGroupKey(const GroupAddress group_address);

 protected:
  Sentinel sentinel_;
  mutable std::mutex sentinel_mutex_;

 private:
  std::vector<SignatureGroup> groups_;
  passport::Pmid our_pmid_;
  DestinationAddress our_destination_;
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
                                             MessageTypeTag tag, SerialisedData message) {

  auto itr = std::find_if(std::begin(groups_), std::end(groups_),
                          [group_address](SignatureGroup group_)
                          { return group_.SignatureGroupAddress() == group_address; });
  if (itr == std::end(groups_)) assert("Sentinel test SimulateMessage: debug Error in test");
  else {
    auto headers = itr->GetHeaders(our_destination_,
                                   MessageId(RandomUint32()), message);
    for (auto header : headers) {
      itr->SaveSentinelReturn(sentinel_.Add(header, tag, message));
    }
  }
}

SentinelReturns SentinelFunctionalTest::GetSentinelReturns(GroupAddress group_address) {
  auto itr = std::find_if(std::begin(groups_), std::end(groups_),
                          [group_address](SignatureGroup group_)
                          { return group_.SignatureGroupAddress() == group_address; });
  if (itr == std::end(groups_)) {
    assert("Sentinel test GetSentinelReturns: debug Error in test");
    return SentinelReturns();
  }
  else {
    return itr->GetSentinelReturns();
  }
}

void SentinelFunctionalTest::SendGetClientKey(Address /*node_address*/) {

}

void SentinelFunctionalTest::SendGetGroupKey(GroupAddress group_address) {
  //std::lock_guard<std::mutex> lock(mutex_);
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
        itr->SaveSentinelReturn(
                    sentinel_.Add(header, MessageTypeTag::GetGroupKeyResponse,
                                  message));
    }
  }
  static_cast<void>(itr);
}

//++++ Free Test Functions +++++++++++++++++++++

size_t CountNoneSentinelReturns(SentinelReturns sentinel_returns) {
  size_t i(0);
  for (auto sentinel_return : sentinel_returns )
      if (sentinel_return == boost::none) i++;
  return i;
}



// first try for specific message type, generalise later
// PutData is chosen as fundamental type with data payload.
TEST_F(SentinelFunctionalTest, BEH_SentinelSimpleAdd) {

  const ImmutableData data(NonEmptyString(RandomBytes(100)));
  PutData put_message(data.TypeId(), Serialise(data));

  // Full group will respond correctly to Sentinel requests
  const GroupAddress group_address(MakeIdentity());
  AddCorrectGroup(group_address, GroupSize, GroupSize, Authority::client_manager);
  SimulateMessage(group_address,
                  MessageTypeTag::PutData, Serialise(put_message));
  auto returns(GetSentinelReturns(group_address));
  EXPECT_EQ(2 * GroupSize -1, CountNoneSentinelReturns(returns));
}

}  // namespacce test

}  // namespace routing

}  // namespace maidsafe

