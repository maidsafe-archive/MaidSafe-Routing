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
#include "maidsafe/common/node_id.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/types.h"
#include "maidsafe/routing/message_header.h"
#include "maidsafe/routing/messages/messages.h"
#include "maidsafe/routing/messages/messages_fwd.h"


namespace maidsafe {

namespace routing {

namespace test {

// structure to track messages sent to sentinel and what sentinel result should be
class SignatureGroup {
 public:
  SignatureGroup(GroupAddress group_address, size_t group_size, size_t active_quorum)
      : group_address_(std::move(group_address)),
        group_size_(std::move(group_size)),
        active_quorum_(std::move(active_quorum)) {
    for (size_t i = 0; i < group_size; i++) {
      nodes_.push_back(passport::CreatePmidAndSigner().first);
    }
  }

  GroupAddress Address() { return group_address_; }

 private:
  GroupAddress group_address_;
  size_t group_size_;
  size_t active_quorum_;
  std::vector<passport::Pmid> nodes_;
};

// persistent sentinel throughout all sentinel tests
class SentinelTest : public testing::Test {
 public:
  SentinelTest()
    : sentinel_([this](Address address) { SendGetClientKey(address); },
                [this](GroupAddress group_address) { SendGetGroupKey(group_address); }) {}

  // Add a correct, cooperative group of indictated total size and responsive quorum
  void AddCorrectGroup(GroupAddress group_address, size_t group_size,
                       size_t active_quorum);

  template <typename TypeData>
  void SimulateMessage(GroupAddress)

  void SendGetClientKey(const Address node_address);
  void SendGetGroupKey(const GroupAddress group_address);

 protected:
  Sentinel sentinel_;
  mutable std::mutex sentinel_mutex_;

 private:
  std::vector<SignatureGroup> groups_;
};

void SentinelTest::AddCorrectGroup(GroupAddress group_address,
                                                size_t group_size,
                                                size_t active_quorum) {
  auto itr = std::find_if(std::begin(groups_), std::end(groups_),
                          [group_address](SignatureGroup group_)
                          { return group_.Address() == group_address; });

  if (itr == std::end(groups_))
      groups_.push_back(SignatureGroup(group_address, group_size, active_quorum));
  else assert("Sentinel test AddCorrectGroup: Group already exists");


  static_cast<void>(itr);
  static_cast<void>(group_address);
  static_cast<void>(group_size);
  static_cast<void>(active_quorum);
}

void SentinelTest::SendGetClientKey(Address /*node_address*/) {

}

void SentinelTest::SendGetGroupKey(GroupAddress group_address) {
  //std::lock_guard<std::mutex> lock(mutex_);
  auto itr = std::find_if(std::begin(groups_), std::end(groups_),
                          [group_address](SignatureGroup group_)
                          { return group_.Address() == group_address; });
  static_cast<void>(itr);
}

// first try for specific message type, generalise later
// PutData is chosen as fundamental type with data payload.
TEST_F(SentinelTest, BEH_SentinelSimpleAdd) {

  const auto serialised_data(RandomString(100));
  PutData put_message(DataTagValue::kImmutableDataValue,
                      std::vector<byte>(serialised_data.begin(),
                                        serialised_data.end()));

  // Full group will respond correctly to Sentinel requests
  const auto group_address(GroupAddress(NodeId(RandomString(NodeId::kSize))));
  AddCorrectGroup(group_address, GroupSize, GroupSize);
  SimulateMessage
}

}  // namespacce test

}  // namespace routing

}  // namespace maidsafe
