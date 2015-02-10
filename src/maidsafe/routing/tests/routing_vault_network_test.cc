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


#include <chrono>
#include <tuple>
#include <vector>

#include "asio/use_future.hpp"
#include "asio/ip/udp.hpp"
#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/path.hpp"
#include "boost/expected/expected.hpp"
#include "eggs/variant.hpp"

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/rsa.h"
#include "maidsafe/common/data_types/immutable_data.h"
#include "maidsafe/common/sqlite3_wrapper.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/routing_node.h"
#include "maidsafe/routing/bootstrap_handler.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/tests/utils/test_utils.h"

namespace maidsafe {

namespace routing {

namespace test {
enum class DataType;
class VaultFacade;

struct MaidManagerAccount {
  MaidManagerAccount() = default;
  MaidManagerAccount(const MaidManagerAccount&) = default;
  MaidManagerAccount(MaidManagerAccount&& other) MAIDSAFE_NOEXCEPT
      : name(std::move(other.name)),
        stored(std::move(other.stored)),
        available(std::move(other.available)) {}
  ~MaidManagerAccount() = default;
  MaidManagerAccount& operator=(const MaidManagerAccount&) = default;
  MaidManagerAccount& operator=(MaidManagerAccount&& rhs) MAIDSAFE_NOEXCEPT {
    name = std::move(rhs.name);
    stored = std::move(rhs.stored);
    available = std::move(rhs.available);
    return *this;
  }

  bool operator==(const MaidManagerAccount& other) const {
    return std::tie(name, stored, available) == std::tie(other.name, other.stored, other.available);
  }
  bool operator!=(const MaidManagerAccount& other) const { return !operator==(other); }
  bool operator<(const MaidManagerAccount& other) const {
    return std::tie(name, stored, available) < std::tie(other.name, other.stored, other.available);
  }
  bool operator>(const MaidManagerAccount& other) { return operator<(other); }
  bool operator<=(const MaidManagerAccount& other) { return !operator>(other); }
  bool operator>=(const MaidManagerAccount& other) { return !operator<(other); }

  Address name;
  uint64_t stored;
  uint64_t available;
};

struct PmidManagerAccount {
  PmidManagerAccount() = default;
  PmidManagerAccount(const PmidManagerAccount&) = default;
  PmidManagerAccount(PmidManagerAccount&& other) MAIDSAFE_NOEXCEPT
      : name(std::move(other.name)),
        stored(std::move(other.stored)),
        available(std::move(other.available)) {}
  ~PmidManagerAccount() = default;
  PmidManagerAccount& operator=(const PmidManagerAccount&) = default;
  PmidManagerAccount& operator=(PmidManagerAccount&& rhs) MAIDSAFE_NOEXCEPT {
    name = std::move(rhs.name);
    stored = std::move(rhs.stored);
    available = std::move(rhs.available);
    return *this;
  }

  bool operator==(const PmidManagerAccount& other) const {
    return std::tie(name, stored, available) == std::tie(other.name, other.stored, other.available);
  }
  bool operator!=(const PmidManagerAccount& other) const { return !operator==(other); }
  bool operator<(const PmidManagerAccount& other) const {
    return std::tie(name, stored, available) < std::tie(other.name, other.stored, other.available);
  }
  bool operator>(const PmidManagerAccount& other) { return operator<(other); }
  bool operator<=(const PmidManagerAccount& other) { return !operator>(other); }
  bool operator>=(const PmidManagerAccount& other) { return !operator<(other); }

  Address name;
  uint64_t stored;
  uint64_t available;
};

template <typename Child>
class MaidManager {
 public:
  using account_type = MaidManagerAccount;
  template <typename T>
  HandlePutPostReturn HandlePut(SourceAddress from, Identity data_name, T data) {
    auto found = static_cast<Child*>(this)->maid_account(std::get<0>(from));
    if (!found)
      return boost::make_unexpected(MakeError(VaultErrors::no_such_account));
    found->stored += data.size();
    static_cast<Child*>(this)
        ->template Put<T>(data_name, data_name, data, [](asio::error_code error) {
          if (error)
            LOG(kWarning) << "could not send from MiadManager (Put)";
        });
  }
  void HandleChurn(CloseGroupDifference) {
    // send all account info to the group of each key and delete it - wait for refreshed accounts
  }
};
template <typename Child>
class VersionManager {};
template <typename Child>
class DataManager {
 public:
  using account_type = MaidManagerAccount;
  template <typename T>
  HandleGetReturn HandleGet(SourceAddress /* from */, Identity /* data_name */) {
    return boost::make_unexpected(MakeError(VaultErrors::failed_to_handle_request));
  }
  template <typename T>
  void HandlePut(SourceAddress /* from */, Identity /* data_name */, DataType /* data */) {}
  void HandleChurn(CloseGroupDifference) {
    // send all account info to the group of each key and delete it - wait for refreshed accounts
  }
};
template <typename DataManagerType, typename VersionManagerType>
class NaeManager {
 public:
  using account_type = MaidManagerAccount;
  template <typename T>
  HandleGetReturn HandleGet(SourceAddress /* from */, Identity /* data_name */) {
    return boost::make_unexpected(MakeError(VaultErrors::failed_to_handle_request));
  }
  template <typename T>
  void HandlePut(SourceAddress /* from */, Identity /* data_name */, DataType /* data */) {}
};  // becomes a dispatcher as its now multiple personas
template <typename Child>
class PmidManager {
 public:
  using account_type = MaidManagerAccount;
  template <typename T>
  HandleGetReturn HandleGet(SourceAddress /* from */, Identity /* data_name */) {
    return boost::make_unexpected(MakeError(VaultErrors::failed_to_handle_request));
  }
  template <typename T>
  void HandlePut(SourceAddress /* from */, Identity /* data_name */, DataType /* data */) {}
  void HandleChurn(CloseGroupDifference) {
    // send all account info to the group of each key and delete it - wait for refreshed accounts
  }
};
template <typename Child>
class PmidNode {
 public:
  using account_type = PmidManagerAccount;
  template <typename T>
  HandleGetReturn HandleGet(SourceAddress /* from */, Identity /* data_name */) {
    return boost::make_unexpected(MakeError(VaultErrors::failed_to_handle_request));
  }
  template <typename T>
  void HandlePut(SourceAddress /* from */, Identity /* data_name */, DataType /* data */) {}
};
class ChurnHandler {};
class GroupClient {};
class RemoteClient {};

namespace fs = boost::filesystem;
// template <typename ClientManager, typename NaeManager, typename NodeManager, typename
// ManagedNode>
class VaultFacade : public test::MaidManager<VaultFacade>,
                    public test::DataManager<VaultFacade>,
                    public test::PmidManager<VaultFacade>,
                    public test::PmidNode<VaultFacade>,
                    public RoutingNode<VaultFacade> {
 public:
  VaultFacade() = delete;
  ~VaultFacade() = default;
  // what functors for Post and Request/Response
  enum class FunctorType { FunctionOne, FunctionTwo };
  enum class DataType { ImmutableData, MutableData, End };
  // what data types are we to handle
  // for data handling this is the types from FromAuthority enumeration // other types will use
  // different
  // path
  // std::tuple<ClientManager, NaeManager, NodeManager> our_personas;  // i.e. handle
  // get/put
  // std::tuple<ImmutableData, MutableData> DataTuple;
  template <typename DataType>
  HandleGetReturn HandleGet(SourceAddress from, Authority authority, DataType data_type,
                            Identity data_name) {
    switch (authority) {
      case Authority::nae_manager:
        if (data_type == DataTagValue::kImmutableDataValue)
          return DataManager::template HandleGet<ImmutableData>(from, data_name);
        else if (data_type == DataTagValue::kMutableDataValue)
          return DataManager::template HandleGet<ImmutableData>(from, data_name);
        break;
      case Authority::node_manager:
        if (data_type == DataTagValue::kImmutableDataValue)
          return PmidManager::template HandleGet<ImmutableData>(from, data_name);
        else if (data_type == DataTagValue::kMutableDataValue)
          return PmidManager::template HandleGet<ImmutableData>(from, data_name);
        break;
      case Authority::managed_node:
        if (data_type == DataTagValue::kImmutableDataValue)
          return PmidNode::template HandleGet<ImmutableData>(from, data_name);
        else if (data_type == DataTagValue::kMutableDataValue)
          return PmidNode::template HandleGet<ImmutableData>(from, data_name);
        break;
      default:
        break;
    }
    return boost::make_unexpected(MakeError(VaultErrors::failed_to_handle_request));
  }
  // template <typename DataType>
  // void HandlePut(SourceAddress from, Authority from_authority, Authority authority,
  //                DataType data_type) {
  //   // switch (authority) {
  //   //   case Authority::nae_manager:
  //   //     if (from_authority != Authority::client_manager)
  //   //       break;
  //   //     if (data_type == DataType::ImmutableData)
  //   //       DataManager::template HandlePut<ImmutableData>(from, data_type);
  //   //     else if (data_type == DataType::MutableData)
  //   //       DataManager::template HandlePut<ImmutableData>(from, data_type);
  //   //     break;
  //   //   case Authority::node_manager:
  //   //     if (data_type == DataType::ImmutableData)
  //   //       PmidManager::template HandlePut<ImmutableData>(from, data_type);
  //   //     else if (data_type == DataType::MutableData)
  //   //       PmidManager::template HandlePut<ImmutableData>(from, data_type);
  //   //     break;
  //   //   case Authority::managed_node:
  //   //     if (data_type == DataType::ImmutableData)
  //   //       PmidNode::template HandlePut<ImmutableData>(from, data_type);
  //   //     else if (data_type == DataType::MutableData)
  //   //       PmidNode::template HandlePut<ImmutableData>(from, data_type);
  //   //     break;
  //   //   default:
  //   //     break;
  //   // }
  // }
  //
  // default no post allowed unless implemented in upper layers
  bool HandlePost(const SerialisedMessage&) { return false; }
  bool HandleGetResponse(const SerialisedMessage&) { return false; }
  // not in local cache do upper layers have it (called when we are in target group)
  // template <typename DataType>
  boost::expected<SerialisedMessage, maidsafe_error> HandleGet(Address) {
    return boost::make_unexpected(MakeError(CommonErrors::no_such_element));
  }
  // default put is allowed unless prevented by upper layers
  bool HandlePut(Address, SerialisedMessage) { return true; }
  // if the implementation allows any put of data in unauthenticated mode
  bool HandleUnauthenticatedPut(Address, SerialisedMessage) { return true; }
  void HandleChurn(CloseGroupDifference diff) {
    MaidManager::HandleChurn(diff);
    DataManager::HandleChurn(diff);
    PmidManager::HandleChurn(diff);
  }

 protected:
  RoutingNode<VaultFacade> routing_node_;
  std::vector<MaidManager::account_type> maid_manager_account;
  std::vector<PmidManager::account_type> pmid_manager_account;
  std::vector<DataManager::account_type> dm_manager_account;
};

TEST(VaultNetworkTest, FUNC_CreateNetPutGetData) {
  // FIXME: The ios seems useless, RUDP has it's own and we don't have any
  // other async actions (same with the tests below).


  LruCache<Identity, SerialisedMessage> cache(0, std::chrono::seconds(0));

  maidsafe::test::TestPath test_dir(
      maidsafe::test::CreateTestPath("RoutingNetworkInit_BEH_ConstructNode"));

  RoutingNode<VaultFacade> n(*test_dir / "node.sqlite3", pmid);

  RoutingNode<VaultFacade> n(*test_dir / "node.sqlite3");

  auto value = NonEmptyString(RandomAlphaNumericString(65));
  Identity key{Identity(crypto::Hash<crypto::SHA512>(value))};
  MutableData a{MutableData::Name(key), value};
  ImmutableData b{value};

  Address from(Address(RandomString(Address::kSize)));
  Address to(Address(RandomString(Address::kSize)));

  n.Get<ImmutableData>(key, [](asio::error_code /* error */) {});
  n.Get<MutableData>(key, [](asio::error_code /* error */) {});

  // n.Put<ImmutableData>(to, b, [](asio::error_code /* error */) {});
  // n.Put<MutableData>(to, a, [](asio::error_code /* error */) {});
}


}  // namespace test

}  // namespace routing

}  // namespace maidsafe
