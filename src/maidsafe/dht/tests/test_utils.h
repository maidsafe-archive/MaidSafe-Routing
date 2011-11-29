/* Copyright (c) 2011 maidsafe.net limited
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
* Neither the name of the maidsafe.net limited nor the names of its
contributors may be used to endorse or promote products derived from this
software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#ifndef MAIDSAFE_DHT_TESTS_TEST_UTILS_H_
#define MAIDSAFE_DHT_TESTS_TEST_UTILS_H_
#include <map>
#include <string>
#include <vector>
#include "boost/thread.hpp"
#include "maidsafe/common/rsa.h"
#include "maidsafe/common/crypto.h"

#ifdef __MSVC__
# pragma warning(push)
# pragma warning(disable: 4127 4244 4267)
#endif
#include "maidsafe/dht/rpcs.pb.h"
#ifdef __MSVC__
# pragma warning(pop)
#endif
#include "maidsafe/dht/routing_table.h"
#include "maidsafe/dht/data_store.h"

namespace maidsafe {

namespace dht {

namespace test {

class AsymGetPublicKeyAndValidation {
 public:
  AsymGetPublicKeyAndValidation(const asymm::Identity &public_key_id,
                                const asymm::PublicKey &public_key,
                                const asymm::PrivateKey &private_key);
  void GetPublicKeyAndValidation(
      const asymm::Identity &public_key_id,
      asymm::GetPublicKeyAndValidationCallback callback);
  void Join();
  bool AddTestValidation(const asymm::Identity &public_key_id,
                         const asymm::PublicKey &public_key);
  void ClearTestValidationMap();

 private:
  void DummyContactValidationGetter(
      asymm::Identity public_key_id,
      asymm::GetPublicKeyAndValidationCallback callback);
  std::map<asymm::Identity, asymm::PublicKey> public_key_id_map_;
  boost::thread_group thread_group_;
};

typedef std::shared_ptr<AsymGetPublicKeyAndValidation> AsymGPKPtr;

const boost::posix_time::milliseconds kNetworkDelay(200);

class CreateContactAndNodeId {
 public:
  explicit CreateContactAndNodeId(uint16_t k);
  virtual ~CreateContactAndNodeId() {}
  NodeId GenerateUniqueRandomId(const NodeId &holder, const int &pos);
  Contact GenerateUniqueContact(const NodeId &holder,
                                const int &pos,
                                const NodeId &target,
                                RoutingTableContactsContainer *generated_nodes);
  NodeId GenerateRandomId(const NodeId &holder, const int &pos);
  Contact ComposeContact(const NodeId &node_id, const Port &port);
  Contact ComposeContactWithKey(const NodeId &node_id,
                                const Port &port,
                                const asymm::Keys &crypto_key);
  void PopulateContactsVector(const int &count,
                              const int &pos,
                              std::vector<Contact> *contacts);
  void set_node_id(NodeId node_id) { node_id_ = node_id; }
 protected:
  Contact contact_;
  NodeId node_id_;
  std::shared_ptr<RoutingTable> routing_table_;
};

KeyValueSignature MakeKVS(const asymm::Keys &rsa_key_pair,
                          const size_t &value_size,
                          std::string key,
                          std::string value);

KeyValueTuple MakeKVT(const asymm::Keys &rsa_key_pair,
                      const size_t &value_size,
                      const bptime::time_duration &ttl,
                      std::string key,
                      std::string value);

protobuf::StoreRequest MakeStoreRequest(
    const Contact &sender,
    const KeyValueSignature &key_value_signature);

protobuf::DeleteRequest MakeDeleteRequest(
    const Contact &sender,
    const KeyValueSignature &key_value_signature);
/*
void JoinNetworkLookup(PrivateKeyPtr private_key);

bool AddTestValidation(PrivateKeyPtr private_key,
                       std::string public_key_id,
                       std::string public_key);
                       */
void AddContact(std::shared_ptr<RoutingTable> routing_table,
                const Contact &contact,
                const RankInfoPtr rank_info);

void SortIds(const NodeId &target_key, std::vector<NodeId> *node_ids);

// returns true if node_id is included in node_ids and is within k closest.
bool WithinKClosest(const NodeId &node_id,
                    const Key &target_key,
                    std::vector<NodeId> node_ids,
                    const uint16_t &k);
void JoinNetworkLookup(KeyPairPtr key_pair);

bool AddTestValidation(KeyPairPtr key_pair,
                       std::string public_key_id,
                       asymm::PublicKey public_key);


void DummyContactValidationGetter(
    asymm::Identity identity,
    asymm::GetPublicKeyAndValidationCallback callback);

bool ValidateFalse(const asymm::PlainText &plain_text,
                   const asymm::Signature &signature,
                   const asymm::PublicKey &public_key);


}  // namespace test

}  // namespace dht

}  // namespace maidsafe

#endif  // MAIDSAFE_DHT_TESTS_TEST_UTILS_H_

