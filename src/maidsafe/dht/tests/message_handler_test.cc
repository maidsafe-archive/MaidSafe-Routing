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
DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include "boost/lexical_cast.hpp"
#include "boost/thread/mutex.hpp"
#include "boost/thread/thread.hpp"

#include "maidsafe/common/test.h"
#include "maidsafe/common/crypto.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/dht/config.h"
#include "maidsafe/dht/message_handler.h"
#include "maidsafe/dht/utils.h"

#ifdef __MSVC__
#  pragma warning(push)
#  pragma warning(disable: 4127 4244 4267)
#endif
#include "maidsafe/dht/rpcs.pb.h"
#include "maidsafe/dht/tests/wrapper.pb.h"
#ifdef __MSVC__
#  pragma warning(pop)
#endif

namespace arg = std::placeholders;

namespace maidsafe {

namespace dht {

namespace test {

class KademliaMessageHandlerTest: public testing::Test {
 public:
  KademliaMessageHandlerTest() : default_public_key_(),
                                 msg_hndlr_(),
                                 msg_hndlr_no_securifier_(PrivateKeyPtr()),
                                 invoked_slots_(),
                                 slots_mutex_() {
    asymm::Keys key_pair;
    asymm::GenerateKeyPair(&key_pair);
    msg_hndlr_.reset(new MessageHandler(
        PrivateKeyPtr(new asymm::PrivateKey(key_pair.private_key))));
    default_public_key_ = key_pair.public_key;
//    Mock asymm::Validate Func to Always return true;
  }
  virtual void SetUp() {}
  virtual void TearDown() {}
  static void SetUpTestCase() { asymm::GenerateKeyPair(&rsa_keypair_); }

  template<class T>
  T GetWrapper(std::string encrypted, std::string key) {
    std::string encrypt_aes_seed = encrypted.substr(1, 512);
    std::string decrypted_message;
    asymm::PrivateKey private_key;
    asymm::DecodePrivateKey(key, &private_key);
    asymm::Decrypt(encrypt_aes_seed, private_key, &decrypted_message);
    encrypt_aes_seed = decrypted_message;
    std::string aes_key = encrypt_aes_seed.substr(0, 32);
    std::string kIV = encrypt_aes_seed.substr(32, 16);
    std::string serialised_message =
        crypto::SymmDecrypt(encrypted.substr(513), aes_key, kIV);
    protobuf::WrapperMessage decrypted_msg;
    decrypted_msg.ParseFromString(serialised_message);
    T result;
    result.ParseFromString(decrypted_msg.payload());
    return result;
  }
  template<class T>
  std::string EncryptMessage(T request,
                             std::string publick_key,
                             MessageType request_type) {
    protobuf::WrapperMessage message;
    message.set_msg_type(request_type);
    message.set_payload(request.SerializeAsString());
    std::string result(1, kAsymmetricEncrypt);
    std::string seed = RandomString(48);
    std::string key = seed.substr(0, 32);
    std::string kIV = seed.substr(32, 16);
    std::string encrypt_message =
        crypto::SymmEncrypt(message.SerializeAsString(), key, kIV);
    std::string encrypt_aes_seed;
    asymm::PublicKey public_key;
    asymm::DecodePublicKey(publick_key, &public_key);
    asymm::Encrypt(seed, public_key, &encrypt_aes_seed);
    result += encrypt_aes_seed + encrypt_message;
    return result;
  }

  void PingRequestSlot(const transport::Info&,
                       const dht::protobuf::PingRequest& request,
                       dht::protobuf::PingResponse* response,
                       transport::Timeout*) {
    boost::mutex::scoped_lock lock(slots_mutex_);
    auto it = invoked_slots_->find(kPingRequest);
    if (it != invoked_slots_->end())
      ++((*it).second);
    response->set_echo(request.ping());
  }

  void PingResponseSlot(const transport::Info&,
                        const dht::protobuf::PingResponse&) {
    boost::mutex::scoped_lock lock(slots_mutex_);
    auto it = invoked_slots_->find(kPingResponse);
    if (it != invoked_slots_->end())
      ++((*it).second);
  }

  void FindValueRequestSlot(const transport::Info&,
                            const dht::protobuf::FindValueRequest&,
                            dht::protobuf::FindValueResponse* response,
                            transport::Timeout*) {
    boost::mutex::scoped_lock lock(slots_mutex_);
    auto it = invoked_slots_->find(kFindValueRequest);
    if (it != invoked_slots_->end())
      ++((*it).second);
    response->set_result(true);
  }

  void FindValueResponseSlot(const transport::Info&,
                             const dht::protobuf::FindValueResponse&) {
    boost::mutex::scoped_lock lock(slots_mutex_);
    auto it = invoked_slots_->find(kFindValueResponse);
    if (it != invoked_slots_->end())
      ++((*it).second);
  }

  void FindNodesRequestSlot(const transport::Info&,
                            const dht::protobuf::FindNodesRequest&,
                            dht::protobuf::FindNodesResponse* response,
                            transport::Timeout*) {
    boost::mutex::scoped_lock lock(slots_mutex_);
    auto it = invoked_slots_->find(kFindNodesRequest);
    if (it != invoked_slots_->end())
      ++((*it).second);
    response->set_result(true);
  }

  void FindNodesResponseSlot(const transport::Info&,
                             const dht::protobuf::FindNodesResponse&) {
    boost::mutex::scoped_lock lock(slots_mutex_);
    auto it = invoked_slots_->find(kFindNodesResponse);
    if (it != invoked_slots_->end())
      ++((*it).second);
  }

  void StoreRequestSlot(const transport::Info&,
                        const dht::protobuf::StoreRequest&,
                        const std::string&, const std::string&,
                        dht::protobuf::StoreResponse* response,
                        transport::Timeout*) {
    boost::mutex::scoped_lock lock(slots_mutex_);
    auto it = invoked_slots_->find(kStoreRequest);
    if (it != invoked_slots_->end())
      ++((*it).second);
    response->set_result(true);
  }

  void StoreResponseSlot(const transport::Info&,
                         const dht::protobuf::StoreResponse&) {
    boost::mutex::scoped_lock lock(slots_mutex_);
    auto it = invoked_slots_->find(kStoreResponse);
    if (it != invoked_slots_->end())
      ++((*it).second);
  }

  void StoreRefreshRequestSlot(
      const transport::Info&,
      const dht::protobuf::StoreRefreshRequest&,
      dht::protobuf::StoreRefreshResponse* response,
      transport::Timeout*) {
    boost::mutex::scoped_lock lock(slots_mutex_);
    auto it = invoked_slots_->find(kStoreRefreshRequest);
    if (it != invoked_slots_->end())
      ++((*it).second);
    response->set_result(true);
  }

  void StoreRefreshResponseSlot(
      const transport::Info&,
      const dht::protobuf::StoreRefreshResponse&) {
    boost::mutex::scoped_lock lock(slots_mutex_);
    auto it = invoked_slots_->find(kStoreRefreshResponse);
    if (it != invoked_slots_->end())
      ++((*it).second);
  }

  void DeleteRequestSlot(const transport::Info&,
                         const dht::protobuf::DeleteRequest&,
                         const std::string&, const std::string&,
                         dht::protobuf::DeleteResponse* response,
                         transport::Timeout*) {
    boost::mutex::scoped_lock lock(slots_mutex_);
    auto it = invoked_slots_->find(kDeleteRequest);
    if (it != invoked_slots_->end())
      ++((*it).second);
    response->set_result(true);
  }

  void DeleteResponseSlot(const transport::Info&,
                          const dht::protobuf::DeleteResponse&) {
    boost::mutex::scoped_lock lock(slots_mutex_);
    auto it = invoked_slots_->find(kDeleteResponse);
    if (it != invoked_slots_->end())
      ++((*it).second);
  }

  void DeleteRefreshRequestSlot(
      const transport::Info&,
      const dht::protobuf::DeleteRefreshRequest&,
      dht::protobuf::DeleteRefreshResponse* response,
      transport::Timeout*) {
    boost::mutex::scoped_lock lock(slots_mutex_);
    auto it = invoked_slots_->find(kDeleteRefreshRequest);
    if (it != invoked_slots_->end())
      ++((*it).second);
    response->set_result(true);
  }

  void DeleteRefreshResponseSlot(
      const transport::Info&,
      const dht::protobuf::DeleteRefreshResponse&) {
    boost::mutex::scoped_lock lock(slots_mutex_);
    auto it = invoked_slots_->find(kDeleteRefreshResponse);
    if (it != invoked_slots_->end())
      ++((*it).second);
  }

  void DownlistNotificationSlot(
      const transport::Info&,
      const dht::protobuf::DownlistNotification&) {
    boost::mutex::scoped_lock lock(slots_mutex_);
    auto it = invoked_slots_->find(kDownlistNotification);
    if (it != invoked_slots_->end())
      ++((*it).second);
  }

  void InitialiseMap() {
    invoked_slots_.reset(new std::map<MessageType, uint16_t>);
    for (int n = kPingRequest; n <= kDownlistNotification; ++n)
      invoked_slots_->insert(std::pair<MessageType, uint16_t>(
                                       MessageType(n), 0));
  }

  void ConnectToHandlerSignals() {
    msg_hndlr_->on_ping_request()->connect(std::bind(
        &KademliaMessageHandlerTest::PingRequestSlot, this, arg::_1, arg::_2,
        arg::_3, arg::_4));
    msg_hndlr_->on_ping_response()->connect(std::bind(
        &KademliaMessageHandlerTest::PingResponseSlot, this, arg::_1, arg::_2));
    msg_hndlr_->on_find_value_request()->connect(std::bind(
        &KademliaMessageHandlerTest::FindValueRequestSlot,
        this, arg::_1, arg::_2, arg::_3, arg::_4));
    msg_hndlr_->on_find_value_response()->connect(std::bind(
        &KademliaMessageHandlerTest::FindValueResponseSlot, this, arg::_1,
        arg::_2));
    msg_hndlr_->on_find_nodes_request()->connect(std::bind(
        &KademliaMessageHandlerTest::FindNodesRequestSlot,
        this, arg::_1, arg::_2, arg::_3, arg::_4));
    msg_hndlr_->on_find_nodes_response()->connect(std::bind(
        &KademliaMessageHandlerTest::FindNodesResponseSlot, this, arg::_1,
        arg::_2));
    msg_hndlr_->on_store_request()->connect(std::bind(
        &KademliaMessageHandlerTest::StoreRequestSlot, this,
        arg::_1, arg::_2, arg::_3, arg::_4, arg::_5, arg::_6));
    msg_hndlr_->on_store_response()->connect(std::bind(
        &KademliaMessageHandlerTest::StoreResponseSlot, this, arg::_1,
        arg::_2));
    msg_hndlr_->on_store_refresh_request()->connect(std::bind(
        &KademliaMessageHandlerTest::StoreRefreshRequestSlot,
        this, arg::_1, arg::_2, arg::_3, arg::_4));
    msg_hndlr_->on_store_refresh_response()->connect(std::bind(
        &KademliaMessageHandlerTest::StoreRefreshResponseSlot, this, arg::_1,
        arg::_2));
    msg_hndlr_->on_delete_request()->connect(std::bind(
        &KademliaMessageHandlerTest::DeleteRequestSlot, this,
        arg::_1, arg::_2, arg::_3, arg::_4, arg::_5, arg::_6));
    msg_hndlr_->on_delete_response()->connect(std::bind(
        &KademliaMessageHandlerTest::DeleteResponseSlot, this, arg::_1,
        arg::_2));
    msg_hndlr_->on_delete_refresh_request()->connect(std::bind(
        &KademliaMessageHandlerTest::DeleteRefreshRequestSlot,
        this, arg::_1, arg::_2, arg::_3, arg::_4));
    msg_hndlr_->on_delete_refresh_response()->connect(std::bind(
        &KademliaMessageHandlerTest::DeleteRefreshResponseSlot, this, arg::_1,
        arg::_2));
    msg_hndlr_->on_downlist_notification()->connect(std::bind(
        &KademliaMessageHandlerTest::DownlistNotificationSlot, this, arg::_1,
        arg::_2));
  }

  std::vector<std::string> CreateMessages() {
    dht::protobuf::PingRequest p_req;
    dht::protobuf::PingResponse p_rsp;
    dht::protobuf::FindValueRequest fv_req;
    dht::protobuf::FindValueResponse fv_rsp;
    dht::protobuf::FindNodesRequest fn_req;
    dht::protobuf::FindNodesResponse fn_rsp;
    dht::protobuf::StoreRequest s_req;
    dht::protobuf::StoreResponse s_rsp;
    dht::protobuf::StoreRefreshRequest sr_req;
    dht::protobuf::StoreRefreshResponse sr_rsp;
    dht::protobuf::DeleteRequest d_req;
    dht::protobuf::DeleteResponse d_rsp;
    dht::protobuf::DeleteRefreshRequest dr_req;
    dht::protobuf::DeleteRefreshResponse dr_rsp;
    dht::protobuf::DownlistNotification dlist;

    dht::protobuf::Contact contact;
    contact.set_node_id("test");
    std::string public_key;
    asymm::EncodePublicKey(default_public_key_, &public_key);
    contact.set_public_key(public_key);
    p_req.set_ping(RandomString(50 + (RandomUint32() % 50)));
    p_req.mutable_sender()->CopyFrom(contact);
    p_rsp.set_echo(p_req.ping());
    fv_req.set_key("fv_key");
    fv_req.mutable_sender()->CopyFrom(contact);
    fv_rsp.set_result(1);
    fn_req.set_key("fn_key");
    fn_req.mutable_sender()->CopyFrom(contact);
    fn_rsp.set_result(1);
    dht::protobuf::SignedValue s_val;
    s_val.set_value("signed_value");
    s_val.set_signature("store_signature");
    s_req.mutable_signed_value()->CopyFrom(s_val);
    s_req.set_ttl(1234);
    s_req.set_key("s_key");
    s_req.mutable_sender()->CopyFrom(contact);
    s_rsp.set_result(1);
    sr_req.mutable_sender()->CopyFrom(contact);
    sr_rsp.set_result(1);
    d_req.mutable_sender()->CopyFrom(contact);
    d_req.set_key("del_key");
    d_req.mutable_signed_value()->CopyFrom(s_val);
    d_rsp.set_result(1);
    dr_req.mutable_sender()->CopyFrom(contact);
    dr_rsp.set_result(1);
    dlist.mutable_sender()->CopyFrom(contact);

    EXPECT_TRUE(p_req.IsInitialized());
    EXPECT_TRUE(p_rsp.IsInitialized());
    EXPECT_TRUE(fv_req.IsInitialized());
    EXPECT_TRUE(fv_rsp.IsInitialized());
    EXPECT_TRUE(fn_req.IsInitialized());
    EXPECT_TRUE(fn_rsp.IsInitialized());
    EXPECT_TRUE(s_req.IsInitialized());
    EXPECT_TRUE(s_rsp.IsInitialized());
    EXPECT_TRUE(sr_req.IsInitialized());
    EXPECT_TRUE(sr_rsp.IsInitialized());
    EXPECT_TRUE(d_req.IsInitialized());
    EXPECT_TRUE(d_rsp.IsInitialized());
    EXPECT_TRUE(dr_req.IsInitialized());
    EXPECT_TRUE(dr_rsp.IsInitialized());
    EXPECT_TRUE(dlist.IsInitialized());

    std::vector<std::string> messages;
    std::string msg_sig;
    protobuf::WrapperMessage wrap;
    wrap.set_msg_type(kPingRequest);
    wrap.set_payload(p_req.SerializeAsString());
    messages.push_back(std::string(1, kAsymmetricEncrypt)
                       + wrap.SerializeAsString());

    wrap.Clear();
    wrap.set_msg_type(kPingResponse);
    wrap.set_payload(p_rsp.SerializeAsString());
    messages.push_back(std::string(1, kAsymmetricEncrypt)
                       + wrap.SerializeAsString());

    wrap.Clear();
    wrap.set_msg_type(kFindValueRequest);
    wrap.set_payload(fv_req.SerializeAsString());
    messages.push_back(std::string(1, kAsymmetricEncrypt)
                       + wrap.SerializeAsString());

    wrap.Clear();
    wrap.set_msg_type(kFindValueResponse);
    wrap.set_payload(fv_rsp.SerializeAsString());
    messages.push_back(std::string(1, kAsymmetricEncrypt)
                       + wrap.SerializeAsString());

    wrap.Clear();
    wrap.set_msg_type(kFindNodesRequest);
    wrap.set_payload(fn_req.SerializeAsString());
    messages.push_back(std::string(1, kAsymmetricEncrypt)
                       + wrap.SerializeAsString());

    wrap.Clear();
    wrap.set_msg_type(kFindNodesResponse);
    wrap.set_payload(fn_rsp.SerializeAsString());
    messages.push_back(std::string(1, kAsymmetricEncrypt)
                       + wrap.SerializeAsString());

    wrap.Clear();
    wrap.set_msg_type(kStoreRequest);
    wrap.set_payload(s_req.SerializeAsString());
    messages.push_back(std::string(1, kSignAndAsymEncrypt)
                       + wrap.SerializeAsString());

    wrap.Clear();
    wrap.set_msg_type(kStoreResponse);
    wrap.set_payload(s_rsp.SerializeAsString());
    messages.push_back(std::string(1, kAsymmetricEncrypt)
                       + wrap.SerializeAsString());

    wrap.Clear();
    wrap.set_msg_type(kStoreRefreshRequest);
    wrap.set_payload(sr_req.SerializeAsString());
    messages.push_back(std::string(1, kSignAndAsymEncrypt)
                       + wrap.SerializeAsString());

    wrap.Clear();
    wrap.set_msg_type(kStoreRefreshResponse);
    wrap.set_payload(sr_rsp.SerializeAsString());
    messages.push_back(std::string(1, kAsymmetricEncrypt)
                       + wrap.SerializeAsString());

    wrap.Clear();
    wrap.set_msg_type(kDeleteRequest);
    wrap.set_payload(d_req.SerializeAsString());
    messages.push_back(std::string(1, kSignAndAsymEncrypt)
                       + wrap.SerializeAsString());

    wrap.Clear();
    wrap.set_msg_type(kDeleteResponse);
    wrap.set_payload(d_rsp.SerializeAsString());
    messages.push_back(std::string(1, kAsymmetricEncrypt)
                       + wrap.SerializeAsString());

    wrap.Clear();
    wrap.set_msg_type(kDeleteRefreshRequest);
    wrap.set_payload(dr_req.SerializeAsString());
    messages.push_back(std::string(1, kSignAndAsymEncrypt)
                       + wrap.SerializeAsString());

    wrap.Clear();
    wrap.set_msg_type(kDeleteRefreshResponse);
    wrap.set_payload(dr_rsp.SerializeAsString());
    messages.push_back(std::string(1, kAsymmetricEncrypt)
                       + wrap.SerializeAsString());

    wrap.Clear();
    wrap.set_msg_type(kDownlistNotification);
    wrap.set_payload(dlist.SerializeAsString());
    messages.push_back(std::string(1, kAsymmetricEncrypt)
                       + wrap.SerializeAsString());

    return messages;
  }

  std::shared_ptr<std::map<MessageType, uint16_t>> invoked_slots() {
    return invoked_slots_;
  }

  void ExecuteThread(std::vector<std::string> messages_copy, int rounds) {
    uint32_t random_sleep((RandomUint32() % 100) + 100);
    for (int a = 0; a < rounds; ++a) {
      Sleep(boost::posix_time::milliseconds(random_sleep));
      for (size_t n = 0; n < messages_copy.size(); ++n) {
        SecurityType security_type = messages_copy[n].at(0);
        protobuf::WrapperMessage wrap;
        wrap.ParseFromString(messages_copy[n].substr(1));
        int message_type = wrap.msg_type();
        std::string payload = wrap.payload();
        transport::Info info;
        std::string *response = new std::string;
        transport::Timeout *timeout = new transport::Timeout;
        std::string message_sig;
        asymm::Sign(boost::lexical_cast<std::string>(message_type) + payload,
                    *msg_hndlr_->private_key_,
                    &message_sig);
        msg_hndlr_->ProcessSerialisedMessage(message_type,
                                             payload,
                                             security_type,
                                             message_sig,
                                             info,
                                             response,
                                             timeout);
      }
    }
  }

 protected:
  asymm::PublicKey default_public_key_;
  std::shared_ptr<MessageHandler> msg_hndlr_;
  MessageHandler msg_hndlr_no_securifier_;
  std::shared_ptr<std::map<MessageType, uint16_t>> invoked_slots_;
  boost::mutex slots_mutex_;
  static asymm::Keys rsa_keypair_;
};

asymm::Keys KademliaMessageHandlerTest::rsa_keypair_;

TEST_F(KademliaMessageHandlerTest, BEH_WrapMessagePingRequest) {
  dht::protobuf::PingRequest ping_rqst;
  ping_rqst.set_ping("ping");
  dht::protobuf::Contact contact;
  contact.set_node_id("test");
  ping_rqst.mutable_sender()->CopyFrom(contact);
  ASSERT_TRUE(ping_rqst.IsInitialized());
  asymm::Keys kp;
  asymm::GenerateKeyPair(&kp);
  std::string result_no_securifier =
      msg_hndlr_no_securifier_.WrapMessage(ping_rqst, kp.public_key);
  ASSERT_EQ("", result_no_securifier);

  std::string function_encrypt =
      msg_hndlr_->WrapMessage(ping_rqst, kp.public_key);
  std::string encode_pub_key;
  asymm::EncodePublicKey(kp.public_key, &encode_pub_key);
  std::string encode_priv_key;
  asymm::EncodePrivateKey(kp.private_key, &encode_priv_key);
  std::string manual_encrypt =
      EncryptMessage<dht::protobuf::PingRequest>(ping_rqst,
                                                 encode_pub_key,
                                                 kPingRequest);
  EXPECT_NE(manual_encrypt, function_encrypt);
//   EXPECT_EQ(manual_encrypt, function_encrypt);

  // decrypt for comparison test
  dht::protobuf::PingRequest decrypted_function_ping =
      GetWrapper<dht::protobuf::PingRequest>(function_encrypt, encode_priv_key);
  dht::protobuf::PingRequest decrypted_manual_ping =
      GetWrapper<dht::protobuf::PingRequest>(manual_encrypt, encode_priv_key);
  ASSERT_EQ(decrypted_manual_ping.ping(), decrypted_function_ping.ping());
  ASSERT_EQ(decrypted_manual_ping.sender().node_id(),
            decrypted_function_ping.sender().node_id());
}

TEST_F(KademliaMessageHandlerTest, BEH_WrapMessageFindValueRequest) {
  dht::protobuf::FindValueRequest value_rqst;
  value_rqst.set_key("request_key");
  dht::protobuf::Contact contact;
  contact.set_node_id("node_id_test");
  value_rqst.mutable_sender()->CopyFrom(contact);
  ASSERT_TRUE(value_rqst.IsInitialized());
  asymm::Keys kp;
  asymm::GenerateKeyPair(&kp);
  std::string result_no_securifier =
      msg_hndlr_no_securifier_.WrapMessage(value_rqst, kp.public_key);
  ASSERT_EQ("", result_no_securifier);

  std::string function_encrypt =
      msg_hndlr_->WrapMessage(value_rqst, kp.public_key);
  std::string encode_pub_key;
  asymm::EncodePublicKey(kp.public_key, &encode_pub_key);
  std::string encode_priv_key;
  asymm::EncodePrivateKey(kp.private_key, &encode_priv_key);
  std::string manual_encrypt =
      EncryptMessage<dht::protobuf::FindValueRequest>(value_rqst,
                                                      encode_pub_key,
                                                      kFindValueRequest);
  EXPECT_NE(manual_encrypt, function_encrypt);
//   EXPECT_EQ(manual_encrypt, function_encrypt);

  // decrypt for comparison test
  dht::protobuf::FindValueRequest decrypted_function_value =
      GetWrapper<dht::protobuf::FindValueRequest>(function_encrypt,
                                                  encode_priv_key);
  dht::protobuf::FindValueRequest decrypted_manual_value =
      GetWrapper<dht::protobuf::FindValueRequest>(manual_encrypt,
                                                  encode_priv_key);
  ASSERT_EQ(decrypted_manual_value.key(), decrypted_function_value.key());
  ASSERT_EQ(decrypted_manual_value.sender().node_id(),
            decrypted_function_value.sender().node_id());
}

TEST_F(KademliaMessageHandlerTest, BEH_WrapMessageFindNodesRequest) {
  dht::protobuf::FindNodesRequest nodes_rqst;
  nodes_rqst.set_key("node_request_key");
  dht::protobuf::Contact contact;
  contact.set_node_id("node_id_test");
  nodes_rqst.mutable_sender()->CopyFrom(contact);
  ASSERT_TRUE(nodes_rqst.IsInitialized());
  asymm::Keys kp;
  asymm::GenerateKeyPair(&kp);
  std::string result_no_securifier =
      msg_hndlr_no_securifier_.WrapMessage(nodes_rqst, kp.public_key);
  ASSERT_EQ("", result_no_securifier);

  std::string function_encrypt =
      msg_hndlr_->WrapMessage(nodes_rqst, kp.public_key);
  std::string encode_pub_key;
  asymm::EncodePublicKey(kp.public_key, &encode_pub_key);
  std::string encode_priv_key;
  asymm::EncodePrivateKey(kp.private_key, &encode_priv_key);
  std::string manual_encrypt =
      EncryptMessage<dht::protobuf::FindNodesRequest>(nodes_rqst,
                                                      encode_pub_key,
                                                      kFindNodesRequest);
  EXPECT_NE(manual_encrypt, function_encrypt);
//   EXPECT_EQ(manual_encrypt, function_encrypt);

  // decrypt for comparison test
  dht::protobuf::FindNodesRequest decrypted_function_value =
      GetWrapper<dht::protobuf::FindNodesRequest>(function_encrypt,
                                                  encode_priv_key);
  dht::protobuf::FindNodesRequest decrypted_manual_value =
      GetWrapper<dht::protobuf::FindNodesRequest>(manual_encrypt,
                                                  encode_priv_key);
  ASSERT_EQ(decrypted_manual_value.key(), decrypted_function_value.key());
  ASSERT_EQ(decrypted_manual_value.sender().node_id(),
            decrypted_function_value.sender().node_id());
}

TEST_F(KademliaMessageHandlerTest, BEH_WrapMessageStoreRequest) {
  dht::protobuf::StoreRequest store_rqst;
  store_rqst.set_key("store_request_key");
  dht::protobuf::Contact contact;
  contact.set_node_id("node_id_test");
  store_rqst.mutable_sender()->CopyFrom(contact);
  dht::protobuf::SignedValue s_val;
  s_val.set_value("signed_value");
  s_val.set_signature("store_signature");
  store_rqst.mutable_signed_value()->CopyFrom(s_val);
  store_rqst.set_ttl(1234);
  ASSERT_TRUE(store_rqst.IsInitialized());
  asymm::Keys kp;
  asymm::GenerateKeyPair(&kp);
  std::string result_no_securifier =
      msg_hndlr_no_securifier_.WrapMessage(store_rqst, kp.public_key);
  ASSERT_EQ("", result_no_securifier);

  std::string function_encrypt =
      msg_hndlr_->WrapMessage(store_rqst, kp.public_key);
  std::string encode_pub_key;
  asymm::EncodePublicKey(kp.public_key, &encode_pub_key);
  std::string encode_priv_key;
  asymm::EncodePrivateKey(kp.private_key, &encode_priv_key);
  std::string manual_encrypt =
      EncryptMessage<dht::protobuf::StoreRequest>(store_rqst,
                                                  encode_pub_key,
                                                  kStoreRequest);
  EXPECT_NE(manual_encrypt, function_encrypt);

  dht::protobuf::StoreRequest decrypted_function_value =
      GetWrapper<dht::protobuf::StoreRequest>(function_encrypt,
                                              encode_priv_key);
  dht::protobuf::StoreRequest decrypted_manual_value =
      GetWrapper<dht::protobuf::StoreRequest>(manual_encrypt,
                                              encode_priv_key);
  ASSERT_EQ(decrypted_manual_value.key(), decrypted_function_value.key());
  ASSERT_EQ(decrypted_manual_value.sender().node_id(),
            decrypted_function_value.sender().node_id());
  ASSERT_EQ(decrypted_manual_value.signed_value().value(),
            decrypted_function_value.signed_value().value());
  ASSERT_EQ(decrypted_manual_value.signed_value().signature(),
            decrypted_function_value.signed_value().signature());
  ASSERT_EQ(decrypted_manual_value.ttl(), decrypted_function_value.ttl());
}

TEST_F(KademliaMessageHandlerTest, BEH_WrapMessageStoreRefreshRequest) {
  dht::protobuf::StoreRefreshRequest refresh_rqst;
  dht::protobuf::Contact contact;
  contact.set_node_id("node_id_test");
  refresh_rqst.mutable_sender()->CopyFrom(contact);
  ASSERT_TRUE(refresh_rqst.IsInitialized());
  asymm::Keys kp;
  asymm::GenerateKeyPair(&kp);
  std::string result_no_securifier =
      msg_hndlr_no_securifier_.WrapMessage(refresh_rqst, kp.public_key);
  ASSERT_EQ("", result_no_securifier);
  std::string function_encrypt =
      msg_hndlr_->WrapMessage(refresh_rqst, kp.public_key);
  std::string encode_pub_key;
  asymm::EncodePublicKey(kp.public_key, &encode_pub_key);
  std::string encode_priv_key;
  asymm::EncodePrivateKey(kp.private_key, &encode_priv_key);
  std::string manual_encrypt =
      EncryptMessage<dht::protobuf::StoreRefreshRequest>(
          refresh_rqst, encode_pub_key, kStoreRefreshRequest);
  EXPECT_NE(manual_encrypt, function_encrypt);
  dht::protobuf::StoreRefreshRequest decrypted_function_value =
      GetWrapper<dht::protobuf::StoreRefreshRequest>(function_encrypt,
                                                     encode_priv_key);
  dht::protobuf::StoreRefreshRequest decrypted_manual_value =
      GetWrapper<dht::protobuf::StoreRefreshRequest>(manual_encrypt,
                                                     encode_priv_key);
  ASSERT_EQ(decrypted_manual_value.sender().node_id(),
            decrypted_function_value.sender().node_id());
}

TEST_F(KademliaMessageHandlerTest, BEH_WrapMessageDeleteRequest) {
  dht::protobuf::DeleteRequest delete_rqst;
  dht::protobuf::Contact contact;
  contact.set_node_id("node_id_test");
  delete_rqst.mutable_sender()->CopyFrom(contact);
  delete_rqst.set_key("delete_request_key");
  dht::protobuf::SignedValue s_val;
  s_val.set_value("signed_value");
  s_val.set_signature("delete_signature");
  delete_rqst.mutable_signed_value()->CopyFrom(s_val);
  ASSERT_TRUE(delete_rqst.IsInitialized());
  asymm::Keys kp;
  asymm::GenerateKeyPair(&kp);
  std::string result_no_securifier =
      msg_hndlr_no_securifier_.WrapMessage(delete_rqst, kp.public_key);
  ASSERT_EQ("", result_no_securifier);
  std::string function_encrypt =
      msg_hndlr_->WrapMessage(delete_rqst, kp.public_key);
  std::string encode_pub_key;
  asymm::EncodePublicKey(kp.public_key, &encode_pub_key);
  std::string encode_priv_key;
  asymm::EncodePrivateKey(kp.private_key, &encode_priv_key);
  std::string manual_encrypt =
      EncryptMessage<dht::protobuf::DeleteRequest>(delete_rqst,
                                                   encode_pub_key,
                                                   kDeleteRequest);
  EXPECT_NE(manual_encrypt, function_encrypt);
//   EXPECT_EQ(manual_encrypt, function_encrypt);
  dht::protobuf::DeleteRequest decrypted_function_value =
      GetWrapper<dht::protobuf::DeleteRequest>(function_encrypt,
                                               encode_priv_key);
  dht::protobuf::DeleteRequest decrypted_manual_value =
      GetWrapper<dht::protobuf::DeleteRequest>(manual_encrypt,
                                               encode_priv_key);
  ASSERT_EQ(decrypted_manual_value.key(), decrypted_function_value.key());
  ASSERT_EQ(decrypted_manual_value.sender().node_id(),
            decrypted_function_value.sender().node_id());
  ASSERT_EQ(decrypted_manual_value.signed_value().value(),
            decrypted_function_value.signed_value().value());
  ASSERT_EQ(decrypted_manual_value.signed_value().signature(),
            decrypted_function_value.signed_value().signature());
}

TEST_F(KademliaMessageHandlerTest, BEH_WrapMessageDeleteRefreshRequest) {
  dht::protobuf::DeleteRefreshRequest delrefresh_rqst;
  dht::protobuf::Contact contact;
  contact.set_node_id("node_id_test");
  delrefresh_rqst.mutable_sender()->CopyFrom(contact);
  ASSERT_TRUE(delrefresh_rqst.IsInitialized());
  asymm::Keys kp;
  asymm::GenerateKeyPair(&kp);
  std::string result_no_securifier =
      msg_hndlr_no_securifier_.WrapMessage(delrefresh_rqst, kp.public_key);
  ASSERT_EQ("", result_no_securifier);
  std::string function_encrypt = msg_hndlr_->WrapMessage(delrefresh_rqst,
                                                        kp.public_key);
  std::string encode_pub_key;
  asymm::EncodePublicKey(kp.public_key, &encode_pub_key);
  std::string encode_priv_key;
  asymm::EncodePrivateKey(kp.private_key, &encode_priv_key);
  std::string manual_encrypt =
      EncryptMessage<dht::protobuf::DeleteRefreshRequest>(
          delrefresh_rqst, encode_pub_key, kDeleteRefreshRequest);
  EXPECT_NE(manual_encrypt, function_encrypt);
  dht::protobuf::DeleteRefreshRequest decrypted_function_value =
      GetWrapper<dht::protobuf::DeleteRefreshRequest>(function_encrypt,
                                                      encode_priv_key);
  dht::protobuf::DeleteRefreshRequest decrypted_manual_value =
      GetWrapper<dht::protobuf::DeleteRefreshRequest>(manual_encrypt,
                                                      encode_priv_key);
  ASSERT_EQ(decrypted_manual_value.sender().node_id(),
            decrypted_function_value.sender().node_id());
}

TEST_F(KademliaMessageHandlerTest, BEH_WrapMessageDownlistNotification) {
  dht::protobuf::DownlistNotification downlist;
  dht::protobuf::Contact contact;
  contact.set_node_id("node_id_test");
  downlist.mutable_sender()->CopyFrom(contact);
  ASSERT_TRUE(downlist.IsInitialized());
  asymm::Keys kp;
  asymm::GenerateKeyPair(&kp);
  std::string result_no_securifier =
      msg_hndlr_no_securifier_.WrapMessage(downlist, kp.public_key);
  ASSERT_EQ("", result_no_securifier);
  std::string function_encrypt = msg_hndlr_->WrapMessage(downlist,
                                                        kp.public_key);
  std::string encode_pub_key;
  asymm::EncodePublicKey(kp.public_key, &encode_pub_key);
  std::string encode_priv_key;
  asymm::EncodePrivateKey(kp.private_key, &encode_priv_key);
  std::string manual_encrypt =
      EncryptMessage<dht::protobuf::DownlistNotification>(
          downlist, encode_pub_key, kDownlistNotification);
  EXPECT_NE(manual_encrypt, function_encrypt);
  dht::protobuf::DownlistNotification decrypted_function_value =
      GetWrapper<dht::protobuf::DownlistNotification>(function_encrypt,
                                                      encode_priv_key);
  dht::protobuf::DownlistNotification decrypted_manual_value =
      GetWrapper<dht::protobuf::DownlistNotification>(manual_encrypt,
                                                      encode_priv_key);
  ASSERT_EQ(decrypted_manual_value.sender().node_id(),
            decrypted_function_value.sender().node_id());
}

TEST_F(KademliaMessageHandlerTest, BEH_WrapMessagePingResponse) {
  dht::protobuf::PingResponse response;
  response.set_echo("ping response echo");
  ASSERT_TRUE(response.IsInitialized());
  asymm::Keys kp;
  asymm::GenerateKeyPair(&kp);
  std::string result_no_securifier =
      msg_hndlr_no_securifier_.WrapMessage(response, kp.public_key);
  ASSERT_EQ("", result_no_securifier);
  std::string function_encrypt = msg_hndlr_->WrapMessage(response,
                                                        kp.public_key);
  std::string encode_pub_key;
  asymm::EncodePublicKey(kp.public_key, &encode_pub_key);
  std::string encode_priv_key;
  asymm::EncodePrivateKey(kp.private_key, &encode_priv_key);
  std::string manual_encrypt =
      EncryptMessage<dht::protobuf::PingResponse>(response,
                                                  encode_pub_key,
                                                  kPingResponse);
  EXPECT_NE(manual_encrypt, function_encrypt);
  dht::protobuf::PingResponse decrypted_function_value =
      GetWrapper<dht::protobuf::PingResponse>(function_encrypt,
                                              encode_priv_key);
  dht::protobuf::PingResponse decrypted_manual_value =
      GetWrapper<dht::protobuf::PingResponse>(manual_encrypt,
                                              encode_priv_key);
  ASSERT_EQ(decrypted_manual_value.echo(),
            decrypted_function_value.echo());
}

TEST_F(KademliaMessageHandlerTest, BEH_WrapMessageFindValueResponse) {
  dht::protobuf::FindValueResponse response;
  response.set_result(1);
  ASSERT_TRUE(response.IsInitialized());
  asymm::Keys kp;
  asymm::GenerateKeyPair(&kp);
  std::string result_no_securifier =
      msg_hndlr_no_securifier_.WrapMessage(response, kp.public_key);
  ASSERT_EQ("", result_no_securifier);
  std::string function_encrypt = msg_hndlr_->WrapMessage(response,
                                                        kp.public_key);
  std::string encode_pub_key;
  asymm::EncodePublicKey(kp.public_key, &encode_pub_key);
  std::string encode_priv_key;
  asymm::EncodePrivateKey(kp.private_key, &encode_priv_key);
  std::string manual_encrypt =
      EncryptMessage<dht::protobuf::FindValueResponse>(response,
                                                       encode_pub_key,
                                                       kFindValueResponse);
  EXPECT_NE(manual_encrypt, function_encrypt);
  dht::protobuf::FindValueResponse decrypted_function_value =
      GetWrapper<dht::protobuf::FindValueResponse>(function_encrypt,
                                                   encode_priv_key);
  dht::protobuf::FindValueResponse decrypted_manual_value =
      GetWrapper<dht::protobuf::FindValueResponse>(manual_encrypt,
                                                   encode_priv_key);
  EXPECT_TRUE(decrypted_manual_value.result());
  EXPECT_TRUE(decrypted_function_value.result());
}

TEST_F(KademliaMessageHandlerTest, BEH_WrapMessageFindNodesResponse) {
  dht::protobuf::FindNodesResponse response;
  response.set_result(1);
  ASSERT_TRUE(response.IsInitialized());
  asymm::Keys kp;
  asymm::GenerateKeyPair(&kp);
  std::string result_no_securifier =
      msg_hndlr_no_securifier_.WrapMessage(response, kp.public_key);
  ASSERT_EQ("", result_no_securifier);
  std::string function_encrypt = msg_hndlr_->WrapMessage(response,
                                                        kp.public_key);
  std::string encode_pub_key;
  asymm::EncodePublicKey(kp.public_key, &encode_pub_key);
  std::string encode_priv_key;
  asymm::EncodePrivateKey(kp.private_key, &encode_priv_key);
  std::string manual_encrypt =
      EncryptMessage<dht::protobuf::FindNodesResponse>(response,
                                                       encode_pub_key,
                                                       kFindNodesResponse);
  EXPECT_NE(manual_encrypt, function_encrypt);
  dht::protobuf::FindNodesResponse decrypted_function_value =
      GetWrapper<dht::protobuf::FindNodesResponse>(function_encrypt,
                                                   encode_priv_key);
  dht::protobuf::FindNodesResponse decrypted_manual_value =
      GetWrapper<dht::protobuf::FindNodesResponse>(manual_encrypt,
                                                   encode_priv_key);
  ASSERT_TRUE(decrypted_manual_value.result());
  ASSERT_TRUE(decrypted_function_value.result());
}

TEST_F(KademliaMessageHandlerTest, BEH_WrapMessageStoreResponse) {
  dht::protobuf::StoreResponse response;
  response.set_result(1);
  ASSERT_TRUE(response.IsInitialized());
  asymm::Keys kp;
  asymm::GenerateKeyPair(&kp);
  std::string result_no_securifier =
      msg_hndlr_no_securifier_.WrapMessage(response, kp.public_key);
  ASSERT_EQ("", result_no_securifier);
  std::string function_encrypt = msg_hndlr_->WrapMessage(response,
                                                        kp.public_key);
  std::string encode_pub_key;
  asymm::EncodePublicKey(kp.public_key, &encode_pub_key);
  std::string encode_priv_key;
  asymm::EncodePrivateKey(kp.private_key, &encode_priv_key);
  std::string manual_encrypt =
      EncryptMessage<dht::protobuf::StoreResponse>(response,
                                                   encode_pub_key,
                                                   kStoreResponse);
  EXPECT_NE(manual_encrypt, function_encrypt);
  dht::protobuf::StoreResponse decrypted_function_value =
      GetWrapper<dht::protobuf::StoreResponse>(function_encrypt,
                                               encode_priv_key);
  dht::protobuf::StoreResponse decrypted_manual_value =
      GetWrapper<dht::protobuf::StoreResponse>(manual_encrypt,
                                               encode_priv_key);
  ASSERT_TRUE(decrypted_manual_value.result());
  ASSERT_TRUE(decrypted_function_value.result());
}

TEST_F(KademliaMessageHandlerTest, BEH_WrapMessageStoreRefreshResponse) {
  dht::protobuf::StoreRefreshResponse response;
  response.set_result(1);
  ASSERT_TRUE(response.IsInitialized());
  asymm::Keys kp;
  asymm::GenerateKeyPair(&kp);
  std::string result_no_securifier =
      msg_hndlr_no_securifier_.WrapMessage(response, kp.public_key);
  ASSERT_EQ("", result_no_securifier);
  std::string function_encrypt = msg_hndlr_->WrapMessage(response,
                                                        kp.public_key);
  std::string encode_pub_key;
  asymm::EncodePublicKey(kp.public_key, &encode_pub_key);
  std::string encode_priv_key;
  asymm::EncodePrivateKey(kp.private_key, &encode_priv_key);
  std::string manual_encrypt =
      EncryptMessage<dht::protobuf::StoreRefreshResponse>(
          response,  encode_pub_key, kStoreRefreshResponse);
  EXPECT_NE(manual_encrypt, function_encrypt);
  dht::protobuf::StoreRefreshResponse decrypted_function_value =
      GetWrapper<dht::protobuf::StoreRefreshResponse>(function_encrypt,
                                                      encode_priv_key);
  dht::protobuf::StoreRefreshResponse decrypted_manual_value =
      GetWrapper<dht::protobuf::StoreRefreshResponse>(manual_encrypt,
                                                      encode_priv_key);
  ASSERT_TRUE(decrypted_manual_value.result());
  ASSERT_TRUE(decrypted_function_value.result());
}

TEST_F(KademliaMessageHandlerTest, BEH_WrapMessageDeleteResponse) {
  dht::protobuf::DeleteResponse response;
  response.set_result(1);
  ASSERT_TRUE(response.IsInitialized());
  asymm::Keys kp;
  asymm::GenerateKeyPair(&kp);
  std::string result_no_securifier =
      msg_hndlr_no_securifier_.WrapMessage(response, kp.public_key);
  ASSERT_EQ("", result_no_securifier);
  std::string function_encrypt = msg_hndlr_->WrapMessage(response,
                                                        kp.public_key);
  std::string encode_pub_key;
  asymm::EncodePublicKey(kp.public_key, &encode_pub_key);
  std::string encode_priv_key;
  asymm::EncodePrivateKey(kp.private_key, &encode_priv_key);
  std::string manual_encrypt =
      EncryptMessage<dht::protobuf::DeleteResponse>(response,
                                                    encode_pub_key,
                                                    kDeleteResponse);
  EXPECT_NE(manual_encrypt, function_encrypt);
  dht::protobuf::DeleteResponse decrypted_function_value =
      GetWrapper<dht::protobuf::DeleteResponse>(function_encrypt,
                                                encode_priv_key);
  dht::protobuf::DeleteResponse decrypted_manual_value =
      GetWrapper<dht::protobuf::DeleteResponse>(manual_encrypt,
                                                encode_priv_key);
  ASSERT_TRUE(decrypted_manual_value.result());
  ASSERT_TRUE(decrypted_function_value.result());
}

TEST_F(KademliaMessageHandlerTest, BEH_WrapMessageDeleteRefreshResponse) {
  dht::protobuf::DeleteRefreshResponse response;
  response.set_result(1);
  ASSERT_TRUE(response.IsInitialized());
  asymm::Keys kp;
  asymm::GenerateKeyPair(&kp);
  std::string result_no_securifier =
      msg_hndlr_no_securifier_.WrapMessage(response, kp.public_key);
  ASSERT_EQ("", result_no_securifier);
  std::string function_encrypt = msg_hndlr_->WrapMessage(response,
                                                        kp.public_key);
  std::string encode_pub_key;
  asymm::EncodePublicKey(kp.public_key, &encode_pub_key);
  std::string encode_priv_key;
  asymm::EncodePrivateKey(kp.private_key, &encode_priv_key);
  std::string manual_encrypt =
      EncryptMessage<dht::protobuf::DeleteRefreshResponse>(
          response, encode_pub_key, kDeleteRefreshResponse);
  EXPECT_NE(manual_encrypt, function_encrypt);
  dht::protobuf::DeleteRefreshResponse decrypted_function_value =
      GetWrapper<dht::protobuf::DeleteRefreshResponse>(function_encrypt,
                                                       encode_priv_key);
  dht::protobuf::DeleteRefreshResponse decrypted_manual_value =
      GetWrapper<dht::protobuf::DeleteRefreshResponse>(manual_encrypt,
                                                       encode_priv_key);
  ASSERT_TRUE(decrypted_manual_value.result());
  ASSERT_TRUE(decrypted_function_value.result());
}

TEST_F(KademliaMessageHandlerTest, BEH_ProcessSerialisedMessagePingRqst) {
  InitialiseMap();
  ConnectToHandlerSignals();
  transport::Info info;
  dht::protobuf::Contact contact;
  contact.set_node_id("test");
  std::string encode_pub_key;
  asymm::EncodePublicKey(rsa_keypair_.public_key, &encode_pub_key);
  contact.set_public_key(encode_pub_key);
  std::string message_signature;
  std::string *message_response = new std::string;
  transport::Timeout *timeout = new transport::Timeout;

  int message_type = kPingRequest;
  dht::protobuf::PingRequest request;
  request.set_ping("ping");
  request.mutable_sender()->CopyFrom(contact);
  std::string payload = request.SerializeAsString();
  ASSERT_TRUE(request.IsInitialized());

  msg_hndlr_->ProcessSerialisedMessage(message_type, payload, kNone,
                                      message_signature, info,
                                      message_response, timeout);
  auto it = invoked_slots_->find(kPingRequest);
  int total = (*it).second;
  ASSERT_EQ(0U, total);
  msg_hndlr_->ProcessSerialisedMessage(message_type, payload,
                                       kAsymmetricEncrypt,
                                       message_signature, info,
                                       message_response, timeout);
  it = invoked_slots_->find(kPingRequest);
  total = (*it).second;
  ASSERT_EQ(1U, total);

  msg_hndlr_->ProcessSerialisedMessage(message_type, payload,
                                      kAsymmetricEncrypt,
                                      message_signature, info,
                                      message_response, timeout);
  it = invoked_slots_->find(kPingRequest);
  total = (*it).second;
  ASSERT_EQ(2U, total);
}

TEST_F(KademliaMessageHandlerTest, BEH_ProcessSerialisedMessagePingRsp) {
  InitialiseMap();
  ConnectToHandlerSignals();
  transport::Info info;
  dht::protobuf::Contact contact;
  contact.set_node_id("test");
  std::string message_signature;
  std::string *message_response = new std::string;
  transport::Timeout *timeout = new transport::Timeout;

  int message_type = kPingResponse;
  dht::protobuf::PingResponse response;
  response.set_echo("ping_echo");
  std::string payload = response.SerializeAsString();
  ASSERT_TRUE(response.IsInitialized());

  msg_hndlr_->ProcessSerialisedMessage(message_type, payload, kNone,
                                      message_signature, info,
                                      message_response, timeout);
  auto it = invoked_slots_->find(kPingResponse);
  int total = (*it).second;
  ASSERT_EQ(0U, total);

  msg_hndlr_->ProcessSerialisedMessage(message_type, payload,
                                      kAsymmetricEncrypt,
                                      message_signature, info,
                                      message_response, timeout);
  it = invoked_slots_->find(kPingResponse);
  total = (*it).second;
  ASSERT_EQ(1U, total);
}

TEST_F(KademliaMessageHandlerTest, BEH_ProcessSerialisedMessageFValRqst) {
  InitialiseMap();
  ConnectToHandlerSignals();
  transport::Info info;
  dht::protobuf::Contact contact;
  contact.set_node_id("test");
  std::string encode_pub_key;
  asymm::EncodePublicKey(rsa_keypair_.public_key, &encode_pub_key);
  contact.set_public_key(encode_pub_key);
  std::string message_signature;
  std::string *message_response = new std::string;
  transport::Timeout *timeout = new transport::Timeout;

  int message_type = kFindValueRequest;
  dht::protobuf::FindValueRequest request;
  request.set_key("FindValue_key");
  request.mutable_sender()->CopyFrom(contact);
  std::string payload = request.SerializeAsString();
  ASSERT_TRUE(request.IsInitialized());

  msg_hndlr_->ProcessSerialisedMessage(message_type, payload, kNone,
                                      message_signature, info,
                                      message_response, timeout);
  auto it = invoked_slots_->find(kFindValueRequest);
  int total = (*it).second;
  ASSERT_EQ(0U, total);

  msg_hndlr_->ProcessSerialisedMessage(message_type, payload,
                                      kAsymmetricEncrypt,
                                      message_signature, info,
                                      message_response, timeout);
  it = invoked_slots_->find(kFindValueRequest);
  total = (*it).second;
  ASSERT_EQ(1U, total);
}

TEST_F(KademliaMessageHandlerTest, BEH_ProcessSerialisedMessageFValRsp) {
  InitialiseMap();
  ConnectToHandlerSignals();
  transport::Info info;
  dht::protobuf::Contact contact;
  contact.set_node_id("test");
  std::string message_signature;
  std::string *message_response = new std::string;
  transport::Timeout *timeout = new transport::Timeout;

  int message_type = kFindValueResponse;
  dht::protobuf::FindValueResponse response;
  response.set_result(1);
  std::string payload = response.SerializeAsString();
  ASSERT_TRUE(response.IsInitialized());

  msg_hndlr_->ProcessSerialisedMessage(message_type, payload, kNone,
                                      message_signature, info,
                                      message_response, timeout);
  auto it = invoked_slots_->find(kFindValueResponse);
  int total = (*it).second;
  ASSERT_EQ(0U, total);

  msg_hndlr_->ProcessSerialisedMessage(message_type, payload,
                                      kAsymmetricEncrypt,
                                      message_signature, info,
                                      message_response, timeout);
  it = invoked_slots_->find(kFindValueResponse);
  total = (*it).second;
  ASSERT_EQ(1U, total);
}

TEST_F(KademliaMessageHandlerTest, BEH_ProcessSerialisedMessageFNodeRqst) {
  InitialiseMap();
  ConnectToHandlerSignals();
  transport::Info info;
  dht::protobuf::Contact contact;
  contact.set_node_id("test");
  std::string encode_pub_key;
  asymm::EncodePublicKey(rsa_keypair_.public_key, &encode_pub_key);
  contact.set_public_key(encode_pub_key);
  std::string message_signature;
  std::string *message_response = new std::string;
  transport::Timeout *timeout = new transport::Timeout;

  int message_type = kFindNodesRequest;
  dht::protobuf::FindNodesRequest request;
  request.set_key("FindNodes_key");
  request.mutable_sender()->CopyFrom(contact);
  std::string payload = request.SerializeAsString();
  ASSERT_TRUE(request.IsInitialized());

  msg_hndlr_->ProcessSerialisedMessage(message_type, payload, kNone,
                                      message_signature, info,
                                      message_response, timeout);
  auto it = invoked_slots_->find(kFindNodesRequest);
  int total = (*it).second;
  ASSERT_EQ(0U, total);

  msg_hndlr_->ProcessSerialisedMessage(message_type, payload,
                                      kAsymmetricEncrypt,
                                      message_signature, info,
                                      message_response, timeout);
  it = invoked_slots_->find(kFindNodesRequest);
  total = (*it).second;
  ASSERT_EQ(1U, total);
}

TEST_F(KademliaMessageHandlerTest, BEH_ProcessSerialisedMessageFNodeRsp) {
  InitialiseMap();
  ConnectToHandlerSignals();
  transport::Info info;
  dht::protobuf::Contact contact;
  contact.set_node_id("test");
  std::string message_signature;
  std::string *message_response = new std::string;
  transport::Timeout *timeout = new transport::Timeout;

  int message_type = kFindNodesResponse;
  dht::protobuf::FindNodesResponse response;
  response.set_result(1);
  std::string payload = response.SerializeAsString();
  ASSERT_TRUE(response.IsInitialized());

  msg_hndlr_->ProcessSerialisedMessage(message_type, payload, kNone,
                                      message_signature, info,
                                      message_response, timeout);
  auto it = invoked_slots_->find(kFindNodesResponse);
  int total = (*it).second;
  ASSERT_EQ(0U, total);

  msg_hndlr_->ProcessSerialisedMessage(message_type, payload,
                                      kAsymmetricEncrypt,
                                      message_signature, info,
                                      message_response, timeout);
  it = invoked_slots_->find(kFindNodesResponse);
  total = (*it).second;
  ASSERT_EQ(1U, total);
}

TEST_F(KademliaMessageHandlerTest, BEH_ProcessSerialisedMessageStoreRqst) {
  InitialiseMap();
  ConnectToHandlerSignals();
  transport::Info info;
  dht::protobuf::Contact contact;
  contact.set_node_id("test");
  std::string encode_pub_key;
  asymm::EncodePublicKey(default_public_key_, &encode_pub_key);
  contact.set_public_key(encode_pub_key);
  std::string message_response;
  transport::Timeout timeout;

  int message_type = kStoreRequest;
  dht::protobuf::StoreRequest request;
  request.set_key("Store_key");
  dht::protobuf::SignedValue s_val;
  s_val.set_value("signed_value");
  s_val.set_signature("store_signature");
  request.mutable_signed_value()->CopyFrom(s_val);
  request.set_ttl(1234);
  request.mutable_sender()->CopyFrom(contact);
  std::string payload = request.SerializeAsString();
  ASSERT_TRUE(request.IsInitialized());

  std::string message_signature;
  ASSERT_EQ(kSuccess,
      asymm::Sign(boost::lexical_cast<std::string>(message_type) + payload,
                  *msg_hndlr_->private_key_,
                  &message_signature));

  // Try with wrong sign/encrypt type
  msg_hndlr_->ProcessSerialisedMessage(message_type, payload, kNone,
                                       message_signature, info,
                                       &message_response, &timeout);
  auto it = invoked_slots_->find(kStoreRequest);
  int total = (*it).second;
  ASSERT_EQ(0U, total);

  // Use correct data
  msg_hndlr_->ProcessSerialisedMessage(message_type, payload,
                                       kSignAndAsymEncrypt, message_signature,
                                       info, &message_response, &timeout);
  it = invoked_slots_->find(kStoreRequest);
  total = (*it).second;
  ASSERT_EQ(1U, total);

  // Try with wrong private key
  asymm::Keys kp;
  asymm::GenerateKeyPair(&kp);
  message_signature.clear();
  ASSERT_EQ(kSuccess,
      asymm::Sign((boost::lexical_cast<std::string>(message_type) + payload),
                  kp.private_key,
                  &message_signature));
  request.mutable_sender()->CopyFrom(contact);
  payload = request.SerializeAsString();
  msg_hndlr_->ProcessSerialisedMessage(message_type, payload,
                                       kSignAndAsymEncrypt, message_signature,
                                       info, &message_response, &timeout);
  it = invoked_slots_->find(kStoreRequest);
  total = (*it).second;
  ASSERT_EQ(1U, total);
}

TEST_F(KademliaMessageHandlerTest, BEH_ProcessSerialisedMessageStoreRsp) {
  InitialiseMap();
  ConnectToHandlerSignals();
  transport::Info info;
  dht::protobuf::Contact contact;
  contact.set_node_id("test");
  std::string message_signature;
  std::string *message_response = new std::string;
  transport::Timeout *timeout = new transport::Timeout;

  int message_type = kStoreResponse;
  dht::protobuf::StoreResponse response;
  response.set_result(1);
  std::string payload = response.SerializeAsString();
  ASSERT_TRUE(response.IsInitialized());

  msg_hndlr_->ProcessSerialisedMessage(message_type, payload, kNone,
                                      message_signature, info,
                                      message_response, timeout);
  auto it = invoked_slots_->find(kStoreResponse);
  int total = (*it).second;
  ASSERT_EQ(0U, total);

  msg_hndlr_->ProcessSerialisedMessage(message_type, payload,
                                      kAsymmetricEncrypt,
                                      message_signature, info,
                                      message_response, timeout);
  it = invoked_slots_->find(kStoreResponse);
  total = (*it).second;
  ASSERT_EQ(1U, total);
}

TEST_F(KademliaMessageHandlerTest, BEH_ProcessSerialisedMessageStoreRefRqst) {
  InitialiseMap();
  ConnectToHandlerSignals();
  transport::Info info;
  dht::protobuf::Contact contact;
  contact.set_node_id("test");
  std::string encode_pub_key;
  asymm::EncodePublicKey(default_public_key_, &encode_pub_key);
  contact.set_public_key(encode_pub_key);
  std::string message_response;
  transport::Timeout timeout;

  int message_type = kStoreRefreshRequest;
  dht::protobuf::StoreRefreshRequest request;
  request.mutable_sender()->CopyFrom(contact);
  std::string payload = request.SerializeAsString();
  ASSERT_TRUE(request.IsInitialized());

  std::string message_signature;
  ASSERT_EQ(kSuccess,
      asymm::Sign(boost::lexical_cast<std::string>(message_type) + payload,
                  *msg_hndlr_->private_key_,
                  &message_signature));

  // Try with wrong sign/encrypt type
  msg_hndlr_->ProcessSerialisedMessage(message_type, payload, kNone,
                                      message_signature, info,
                                      &message_response, &timeout);
  auto it = invoked_slots_->find(kStoreRefreshRequest);
  int total = (*it).second;
  ASSERT_EQ(0U, total);

  // Use correct data
  msg_hndlr_->ProcessSerialisedMessage(message_type, payload,
                                       kSignAndAsymEncrypt, message_signature,
                                       info, &message_response, &timeout);
  it = invoked_slots_->find(kStoreRefreshRequest);
  total = (*it).second;
  ASSERT_EQ(1U, total);

  // Try with wrong private key
  message_signature.clear();
  asymm::Keys kp;
  asymm::GenerateKeyPair(&kp);
  ASSERT_EQ(kSuccess,
      asymm::Sign((boost::lexical_cast<std::string>(message_type) + payload),
                  kp.private_key,
                  &message_signature));
  request.mutable_sender()->CopyFrom(contact);
  payload = request.SerializeAsString();
  msg_hndlr_->ProcessSerialisedMessage(message_type, payload,
                                       kSignAndAsymEncrypt, message_signature,
                                       info, &message_response, &timeout);
  it = invoked_slots_->find(kStoreRefreshRequest);
  total = (*it).second;
  ASSERT_EQ(1U, total);
}

TEST_F(KademliaMessageHandlerTest, BEH_ProcessSerialisedMessageStoreRefRsp) {
  InitialiseMap();
  ConnectToHandlerSignals();
  transport::Info info;
  dht::protobuf::Contact contact;
  contact.set_node_id("test");
  std::string message_signature;
  std::string *message_response = new std::string;
  transport::Timeout *timeout = new transport::Timeout;

  int message_type = kStoreRefreshResponse;
  dht::protobuf::StoreRefreshResponse response;
  response.set_result(1);
  std::string payload = response.SerializeAsString();
  ASSERT_TRUE(response.IsInitialized());

  msg_hndlr_->ProcessSerialisedMessage(message_type, payload, kNone,
                                      message_signature, info,
                                      message_response, timeout);
  auto it = invoked_slots_->find(kStoreRefreshResponse);
  int total = (*it).second;
  ASSERT_EQ(0U, total);

  msg_hndlr_->ProcessSerialisedMessage(message_type, payload,
                                      kAsymmetricEncrypt,
                                      message_signature, info,
                                      message_response, timeout);
  it = invoked_slots_->find(kStoreRefreshResponse);
  total = (*it).second;
  ASSERT_EQ(1U, total);
}

TEST_F(KademliaMessageHandlerTest, BEH_ProcessSerialisedMessageDeleteRqst) {
  InitialiseMap();
  ConnectToHandlerSignals();
  transport::Info info;
  dht::protobuf::Contact contact;
  contact.set_node_id("test");
  std::string encode_pub_key;
  asymm::EncodePublicKey(default_public_key_, &encode_pub_key);
  contact.set_public_key(encode_pub_key);
  std::string message_response;
  transport::Timeout timeout;

  int message_type = kDeleteRequest;
  dht::protobuf::DeleteRequest request;
  request.set_key("Store_key");
  dht::protobuf::SignedValue s_val;
  s_val.set_value("signed_value");
  s_val.set_signature("store_signature");
  request.mutable_signed_value()->CopyFrom(s_val);
  request.mutable_sender()->CopyFrom(contact);
  std::string payload = request.SerializeAsString();
  ASSERT_TRUE(request.IsInitialized());

  std::string message_signature;
  ASSERT_EQ(kSuccess,
      asymm::Sign(boost::lexical_cast<std::string>(message_type) + payload,
                  *msg_hndlr_->private_key_,
                  &message_signature));

  // Try with wrong sign/encrypt type
  msg_hndlr_->ProcessSerialisedMessage(message_type, payload, kNone,
                                       message_signature, info,
                                       &message_response, &timeout);
  auto it = invoked_slots_->find(kDeleteRequest);
  int total = (*it).second;
  ASSERT_EQ(0U, total);

  // Use correct data
  msg_hndlr_->ProcessSerialisedMessage(message_type, payload,
                                      kSignAndAsymEncrypt, message_signature,
                                      info, &message_response, &timeout);
  it = invoked_slots_->find(kDeleteRequest);
  total = (*it).second;
  ASSERT_EQ(1U, total);

  message_signature.clear();
  asymm::Keys kp;
  asymm::GenerateKeyPair(&kp);
  asymm::Sign((boost::lexical_cast<std::string>(message_type) + payload),
              kp.private_key,
              &message_signature);
  request.mutable_sender()->CopyFrom(contact);
  payload = request.SerializeAsString();
  msg_hndlr_->ProcessSerialisedMessage(message_type, payload,
                                       kSignAndAsymEncrypt, message_signature,
                                       info, &message_response, &timeout);
  it = invoked_slots_->find(kDeleteRequest);
  total = (*it).second;
  ASSERT_EQ(1U, total);
}

TEST_F(KademliaMessageHandlerTest, BEH_ProcessSerialisedMessageDeleteRsp) {
  InitialiseMap();
  ConnectToHandlerSignals();
  transport::Info info;
  dht::protobuf::Contact contact;
  contact.set_node_id("test");
  std::string message_signature;
  std::string *message_response = new std::string;
  transport::Timeout *timeout = new transport::Timeout;

  int message_type = kDeleteResponse;
  dht::protobuf::DeleteResponse response;
  response.set_result(1);
  std::string payload = response.SerializeAsString();
  ASSERT_TRUE(response.IsInitialized());

  msg_hndlr_->ProcessSerialisedMessage(message_type, payload, kNone,
                                      message_signature, info,
                                      message_response, timeout);
  auto it = invoked_slots_->find(kDeleteResponse);
  int total = (*it).second;
  ASSERT_EQ(0U, total);

  msg_hndlr_->ProcessSerialisedMessage(message_type, payload,
                                      kAsymmetricEncrypt,
                                      message_signature, info,
                                      message_response, timeout);
  it = invoked_slots_->find(kDeleteResponse);
  total = (*it).second;
  ASSERT_EQ(1U, total);
}

TEST_F(KademliaMessageHandlerTest, BEH_ProcessSerialisedMessageDeleteRefRqst) {
  InitialiseMap();
  ConnectToHandlerSignals();
  transport::Info info;
  dht::protobuf::Contact contact;
  contact.set_node_id("test");
  std::string encode_pub_key;
  asymm::EncodePublicKey(default_public_key_, &encode_pub_key);
  contact.set_public_key(encode_pub_key);
  std::string message_response;
  transport::Timeout timeout;

  int message_type = kDeleteRefreshRequest;
  dht::protobuf::DeleteRefreshRequest request;
  request.mutable_sender()->CopyFrom(contact);
  std::string payload = request.SerializeAsString();
  ASSERT_TRUE(request.IsInitialized());

  std::string message_signature;
  ASSERT_EQ(kSuccess,
      asymm::Sign(boost::lexical_cast<std::string>(message_type) + payload,
                  *msg_hndlr_->private_key_,
                  &message_signature));

  // Try with wrong sign/encrypt type
  msg_hndlr_->ProcessSerialisedMessage(message_type, payload, kNone,
                                       message_signature, info,
                                       &message_response, &timeout);
  auto it = invoked_slots_->find(kDeleteRefreshRequest);
  int total = (*it).second;
  ASSERT_EQ(0U, total);

  // Use correct data
  msg_hndlr_->ProcessSerialisedMessage(message_type, payload,
                                       kSignAndAsymEncrypt, message_signature,
                                       info, &message_response, &timeout);
  it = invoked_slots_->find(kDeleteRefreshRequest);
  total = (*it).second;
  ASSERT_EQ(1U, total);

  // Try with wrong private key
  message_signature.clear();
  asymm::Keys kp;
  asymm::GenerateKeyPair(&kp);
  ASSERT_EQ(kSuccess,
      asymm::Sign((boost::lexical_cast<std::string>(message_type) + payload),
                  kp.private_key,
                  &message_signature));
  request.mutable_sender()->CopyFrom(contact);
  payload = request.SerializeAsString();
  msg_hndlr_->ProcessSerialisedMessage(message_type, payload,
                                       kSignAndAsymEncrypt,
                                       message_signature, info,
                                       &message_response, &timeout);
  it = invoked_slots_->find(kDeleteRefreshRequest);
  total = (*it).second;
  ASSERT_EQ(1U, total);
}

TEST_F(KademliaMessageHandlerTest, BEH_ProcessSerialisedMessageDeleteRefRsp) {
  InitialiseMap();
  ConnectToHandlerSignals();
  transport::Info info;
  dht::protobuf::Contact contact;
  contact.set_node_id("test");
  std::string message_signature;
  std::string *message_response = new std::string;
  transport::Timeout *timeout = new transport::Timeout;

  int message_type = kDeleteRefreshResponse;
  dht::protobuf::DeleteRefreshResponse response;
  response.set_result(1);
  std::string payload = response.SerializeAsString();
  ASSERT_TRUE(response.IsInitialized());

  msg_hndlr_->ProcessSerialisedMessage(message_type, payload, kNone,
                                      message_signature, info,
                                      message_response, timeout);
  auto it = invoked_slots_->find(kDeleteRefreshResponse);
  int total = (*it).second;
  ASSERT_EQ(0U, total);

  msg_hndlr_->ProcessSerialisedMessage(message_type, payload,
                                      kAsymmetricEncrypt,
                                      message_signature, info,
                                      message_response, timeout);
  it = invoked_slots_->find(kDeleteRefreshResponse);
  total = (*it).second;
  ASSERT_EQ(1U, total);
}

TEST_F(KademliaMessageHandlerTest, BEH_ProcessSerialisedMessageDownlist) {
  InitialiseMap();
  ConnectToHandlerSignals();
  transport::Info info;
  dht::protobuf::Contact contact;
  contact.set_node_id("test");
  std::string encode_pub_key;
  asymm::EncodePublicKey(rsa_keypair_.public_key, &encode_pub_key);
  contact.set_public_key(encode_pub_key);
  std::string message_signature;
  std::string *message_response = new std::string;
  transport::Timeout *timeout = new transport::Timeout;

  int message_type = kDownlistNotification;
  dht::protobuf::DownlistNotification notification;
  notification.mutable_sender()->CopyFrom(contact);
  std::string payload = notification.SerializeAsString();
  ASSERT_TRUE(notification.IsInitialized());

  msg_hndlr_->ProcessSerialisedMessage(message_type, payload, kNone,
                                      message_signature, info,
                                      message_response, timeout);
  auto it = invoked_slots_->find(kDownlistNotification);
  int total = (*it).second;
  ASSERT_EQ(0U, total);

  msg_hndlr_->ProcessSerialisedMessage(message_type, payload,
                                      kAsymmetricEncrypt,
                                      message_signature, info,
                                      message_response, timeout);
  it = invoked_slots_->find(kDownlistNotification);
  total = (*it).second;
  ASSERT_EQ(1U, total);
}

TEST_F(KademliaMessageHandlerTest, FUNC_ThreadedMessageHandling) {
  ConnectToHandlerSignals();
  InitialiseMap();
  std::vector<std::string> messages(CreateMessages());
  uint8_t thread_count((RandomUint32() % 5) + 4);
  uint16_t total_messages(0);
  boost::thread_group thg;
  for (uint8_t n = 0; n < thread_count; ++n) {
    uint16_t rounds((RandomUint32() % 5) + 4);
    thg.create_thread(std::bind(&KademliaMessageHandlerTest::ExecuteThread,
                                this, messages, rounds));
    total_messages += rounds;
  }

  thg.join_all();
  std::shared_ptr<std::map<MessageType, uint16_t>> slots = invoked_slots();
  for (auto it = slots->begin(); it != slots->end(); ++it)
    ASSERT_EQ(uint16_t(total_messages), (*it).second);
}

}  // namespace test_service

}  // namespace dht

}  // namespace maidsafe
