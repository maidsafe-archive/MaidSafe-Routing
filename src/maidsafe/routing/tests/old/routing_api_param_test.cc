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

#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <mutex>
#include <vector>

#include "boost/asio.hpp"
#include "boost/exception/all.hpp"
#include "boost/filesystem/exception.hpp"
#include "boost/progress.hpp"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
#include "boost/thread/future.hpp"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "maidsafe/common/Address.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/rudp/parameters.h"

#include "maidsafe/passport/passport.h"

#include "maidsafe/routing/bootstrap_file_operations.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing_api.h"
#include "maidsafe/routing/routing_impl.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/tests/test_utils.h"

namespace maidsafe {

namespace routing {

namespace test {

namespace bptime = boost::posix_time;
namespace fs = boost::filesystem;

namespace {

typedef boost::asio::ip::udp::endpoint Endpoint;

const int kClientCount(4);
const int kServerCount(10);
const int kNetworkSize = kClientCount + kServerCount;
MessageReceivedFunctor no_ops_message_received_functor =
    [](const std::string&, ReplyFunctor) {};  // NOLINT

}  // anonymous namespace

class RoutingApi : public testing::TestWithParam<unsigned int> {
 public:
  RoutingApi() : kDataSize_(GetParam()) {}

 protected:
  const unsigned int kDataSize_;
};

TEST_P(RoutingApi, FUNC_API_SendGroup) {
// Currently the tests with larger data size pass on non-Windows platforms.  We can't disable the
// larger values by moving the macro inside a preprocessor if/else block since CMake's C++ parsing
// can't handle this and will try and add the test twice.
// TODO(Team) BEFORE_RELEASE - Re-enable for Windows.
#ifdef MAIDSAFE_WIN32
  if (kDataSize_ > 128 * 1024)
    return GTEST_SUCCEED();
#endif

#if !defined(NDEBUG)
  if (kDataSize_ > 1024 * 1024)
    return GTEST_SUCCEED();
#endif

#if (__GNUC__ < 4 || \
     (__GNUC__ == 4 && (__GNUC_MINOR__ < 8 || (__GNUC_MINOR__ == 8 && __GNUC_PATCHLEVEL__ < 3))))
  if (kDataSize_ > 128 * 1024)
    return GTEST_SUCCEED();
#endif

  Endpoint endpoint1(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort()),
      endpoint2(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort());
  ScopedBootstrapFile bootstrap_file({endpoint1, endpoint2});


  auto timeout(Parameters::default_response_timeout);
  const unsigned int kMessageCount(10);  // nodes send kMessageCount messages to other nodes
  Parameters::default_response_timeout *= kDataSize_ / 1024;
  int min_join_status(std::min(kServerCount, 8));
  std::vector<boost::promise<bool>> join_promises(kNetworkSize);
  std::vector<boost::future<bool>> join_futures;
  std::deque<bool> promised;
  std::vector<NetworkStatusFunctor> status_vector;
  std::mutex mutex;
  Functors functors;

  std::vector<NodeInfoAndPrivateKey> nodes;
  std::map<Address, asymm::PublicKey> key_map;
  std::vector<std::shared_ptr<Routing>> routing_node;
  int i(0);
  functors.request_public_key = [&](const Address& Address, GivePublicKeyFunctor give_key) {
    LOG(kWarning) << "node_validation called for " << Address;
    auto itr(key_map.find(Address));
    if (key_map.end() != itr)
      give_key((*itr).second);
  };

  for (; i != kServerCount; ++i) {
    auto pmid(passport::CreatePmidAndSigner().first);
    NodeInfoAndPrivateKey node(MakeNodeInfoAndKeysWithPmid(pmid));
    nodes.push_back(node);
    key_map.insert(std::make_pair(node.node_info.id, pmid.public_key()));
    routing_node.push_back(std::make_shared<Routing>(pmid));
  }

  functors.network_status = [](int) {};  // NOLINT

  functors.message_and_caching.message_received =
      [&](const std::string& message, ReplyFunctor reply_functor) {
    reply_functor("response to " + message);
  };

  auto a1 = boost::async(boost::launch::async, [&] {
    return routing_node[0]->ZeroStateJoin(functors, endpoint1, endpoint2, nodes[1].node_info);
  });
  auto a2 = boost::async(boost::launch::async, [&] {
    return routing_node[1]->ZeroStateJoin(functors, endpoint2, endpoint1, nodes[0].node_info);
  });

  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !

  // Ignoring 2 zero state nodes
  promised.push_back(false);
  promised.push_back(false);
  status_vector.emplace_back([](int /*x*/) {});
  status_vector.emplace_back([](int /*x*/) {});
  boost::promise<bool> promise1, promise2;
  join_futures.emplace_back(promise1.get_future());
  join_futures.emplace_back(promise2.get_future());

  // Joining remaining server nodes
  for (auto i(2); i != (kServerCount); ++i) {
    join_futures.emplace_back(join_promises.at(i).get_future());
    promised.push_back(true);
    status_vector.emplace_back([=, &join_promises, &mutex, &promised](int result) {
      ASSERT_GE(result, kSuccess);
      if (result == NetworkStatus(false, std::min(i, min_join_status))) {
        std::lock_guard<std::mutex> lock(mutex);
        if (promised.at(i)) {
          join_promises.at(i).set_value(true);
          promised.at(i) = false;
        }
      }
    });
  }

  for (auto i(2); i != kServerCount; ++i) {
    functors.network_status = status_vector.at(i);
    routing_node[i]->Join(functors);
    EXPECT_EQ(join_futures.at(i).wait_for(boost::chrono::seconds(10)), boost::future_status::ready);
  }
  std::string data(RandomAlphaNumericString(this->kDataSize_));
  // Call SendGroup repeatedly - measure duration to allow performance comparison
  boost::progress_timer t;

  std::mutex send_mutex;
  std::vector<boost::promise<bool>> send_promises(kServerCount * kMessageCount);
  std::vector<unsigned int> send_counts(kServerCount * kMessageCount, 0);
  std::vector<boost::future<bool>> send_futures;
  for (auto& send_promise : send_promises)
    send_futures.emplace_back(send_promise.get_future());
  bool result(false);
  for (unsigned int i(0); i < kServerCount; ++i) {
    Address dest_id(routing_node[i]->kNodeId());
    unsigned int count(0);
    while (count < kMessageCount) {
      unsigned int message_index(i * kServerCount + count);
      ResponseFunctor response_functor =
          [&send_mutex, &send_promises, &send_counts, &data, message_index, &result](
              std::string string) {
        std::unique_lock<std::mutex> lock(send_mutex);
        EXPECT_EQ("response to " + data, string) << "for message_index " << message_index;
        if (send_counts.at(message_index) >= Parameters::group_size)
          return;
        if (string != "response to " + data) {
          result = false;
        } else {
          result = true;
        }
        send_counts.at(message_index) += 1;
        if (send_counts.at(message_index) == Parameters::group_size)
          send_promises.at(message_index).set_value(result);
      };

      routing_node[i]->SendGroup(dest_id, data, false, response_functor);
      ++count;
    }
  }

  while (!send_futures.empty()) {
    send_futures.erase(std::remove_if(send_futures.begin(), send_futures.end(),
                                      [&data](boost::future<bool>& future_bool) -> bool {
                         if (future_bool.wait_for(boost::chrono::seconds::zero()) ==
                             boost::future_status::ready) {
                           EXPECT_TRUE(future_bool.get());
                           return true;
                         } else {
                           return false;
                         }
                       }),
                       send_futures.end());
    std::this_thread::yield();
  }
  auto time_taken(t.elapsed());
  std::cout << "\n Time taken: " << time_taken
            << "\n Total number of request messages :" << (kMessageCount * kServerCount)
            << "\n Total number of response messages :" << (kMessageCount * kServerCount * 4)
            << "\n Message size : " << (this->kDataSize_ / 1024) << "kB \n";
  Parameters::default_response_timeout = timeout;
}

INSTANTIATE_TEST_CASE_P(SendGroup, RoutingApi,
                        testing::Values(128 * 1024, 256 * 1024, 512 * 1024, 1024 * 1024,
                                        Parameters::max_data_size));

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
