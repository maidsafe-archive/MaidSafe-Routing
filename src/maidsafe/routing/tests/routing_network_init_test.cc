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
class VaultFacade;
template <typename Child>
class MaidManager {
 public:
  template <typename T>
  void HandleGet(SourceAddress /* from */, Identity /* data_name */) {}
  void HandleChurn(CloseGroupDifference) {
    // send all account info to the group of each key and delete it - wait for refreshed accounts
  }
};
template <typename Child>
class VersionManager {};
template <typename Child>
class DataManager {
 public:
  template <typename T>
  void HandleGet(SourceAddress from, Identity data_name) {
    // FIXME(dirvine) We need to pass along the full source address to retain the ReplyTo field
    // :01/02/2015
    static_cast<Child*>(this)
        ->template Get<T>(data_name, std::get<0>(from), [](asio::error_code error) {
          if (error)
            LOG(kWarning) << "could not send from datamanager ";
        });
  }
  void HandleChurn(CloseGroupDifference) {
    // send all account info to the group of each key and delete it - wait for refreshed accounts
  }
};
template <typename DataManagerType, typename VersionManagerType>
class NaeManager {
 public:
  template <typename T>
  void HandleGet(SourceAddress from, Identity data_name);
};  // becomes a dispatcher as its now multiple personas
template <typename Child>
class PmidManager {
 public:
  template <typename T>
  void HandleGet(SourceAddress /* from */, Identity /* data_name */) {}
  void HandleChurn(CloseGroupDifference) {
    // send all account info to the group of each key and delete it - wait for refreshed accounts
  }
};
template <typename Child>
class PmidNode {
 public:
  template <typename T>
  void HandleGet(SourceAddress /* from */, Identity /* data_name */) {}
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
  VaultFacade() = default;
  ~VaultFacade() = default;
  // what functors for Post and Request/Response
  enum class FunctorType { FunctionOne, FunctionTwo };
  // what data types are we to handle
  enum class DataType { ImmutableData, MutableData, End };
  // for data handling this is the types from FromAuthority enumeration // other types will use
  // different
  // path
  // std::tuple<ClientManager, NaeManager, NodeManager> our_personas;  // i.e. handle
  // get/put
  // std::tuple<ImmutableData, MutableData> DataTuple;
  template <typename DataType>
  // void Get(Identity /* name */, Address /* from */) {}
  void HandleGet(SourceAddress from, Authority authority, DataType data_type, Identity data_name) {
    switch (authority) {
      case Authority::nae_manager:
        if (data_type == DataType::ImmutableData)
          DataManager::template HandleGet<ImmutableData>(from, data_name);
        else if (data_type == DataType::MutableData)
          DataManager::template HandleGet<ImmutableData>(from, data_name);
        break;
      case Authority::node_manager:
        if (data_type == DataType::ImmutableData)
          PmidManager::template HandleGet<ImmutableData>(from, data_name);
        else if (data_type == DataType::MutableData)
          PmidManager::template HandleGet<ImmutableData>(from, data_name);
        break;
      case Authority::managed_node:
        if (data_type == DataType::ImmutableData)
          PmidNode::template HandleGet<ImmutableData>(from, data_name);
        else if (data_type == DataType::MutableData)
          PmidNode::template HandleGet<ImmutableData>(from, data_name);
        break;
      default:
        break;
    }
  }

  // default no post allowed unless implemented in upper layers
  bool HandlePost(const SerialisedMessage&) { return false; }
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

 private:
  // RoutingNode routing_node_;
};

TEST(VaultNetworkTest, FUNC_CreateNetPutGetData) {
  // FIXME: The ios seems useless, RUDP has it's own and we don't have any
  // other async actions (same with the tests below).
  asio::io_service ios;

  passport::Pmid pmid = passport::CreatePmidAndSigner().first;

  LruCache<Identity, SerialisedMessage> cache(0, std::chrono::seconds(0));

  maidsafe::test::TestPath test_dir(
      maidsafe::test::CreateTestPath("RoutingNetworkInit_BEH_ConstructNode"));


  RoutingNode<VaultFacade> n(ios, *test_dir / "node.sqlite3", pmid);
  auto value = NonEmptyString(RandomAlphaNumericString(65));

  Identity key{Identity(crypto::Hash<crypto::SHA512>(value))};
  MutableData a{MutableData::Name(key), value};
  ImmutableData b{value};

  Address from(Address(RandomString(Address::kSize)));
  Address to(Address(RandomString(Address::kSize)));

  n.Get<ImmutableData>(key, from, [](asio::error_code /* error */) {});
  n.Get<MutableData>(key, from, [](asio::error_code /* error */) {});

  n.Put<ImmutableData>(to, b, [](asio::error_code /* error */) {});
  n.Put<MutableData>(to, a, [](asio::error_code /* error */) {});
}

// TEST(RoutingNetworkInit, BEH_InitTwo) {
//   using rudp::Endpoint;
//   using rudp::Contact;
//   using rudp::EndpointPair;
//   using std::make_shared;
//
//   asio::io_service ios;
//
//   passport::Pmid pmid1 = passport::CreatePmidAndSigner().first;
//   passport::Pmid pmid2 = passport::CreatePmidAndSigner().first;
//
//   LruCache<Identity, SerialisedMessage> cache1(0, std::chrono::seconds(0));
//   LruCache<Identity, SerialisedMessage> cache2(0, std::chrono::seconds(0));
//
//   maidsafe::test::TestPath test_dir(
//       maidsafe::test::CreateTestPath("RoutingNetworkInit_BEH_InitTwo"));
//
//   auto n1 = make_shared<RoutingNode<VaultFacade>>(ios, *test_dir / "node1.sqlite3", pmid1);
//   auto n2 = make_shared<RoutingNode<VaultFacade>>(ios, *test_dir / "node2.sqlite3", pmid2);
//
//   EndpointPair endpoints1(Endpoint(GetLocalIp(), maidsafe::test::GetRandomPort()));
//   EndpointPair endpoints2(Endpoint(GetLocalIp(), maidsafe::test::GetRandomPort()));
//
//   // n1->AddBootstrapContact(n2->MakeContact(endpoints2));
//   // n2->AddBootstrapContact(n1->MakeContact(endpoints1));
//
//   // auto boot_future1 = n1->Bootstrap(endpoints1.local, asio::use_future);
//   // auto boot_future2 = n2->Bootstrap(endpoints2.local, asio::use_future);
//   //
//   // boot_future1.get();
//   // boot_future2.get();
// }
//
}  // namespace test

}  // namespace routing

}  // namespace maidsafe
