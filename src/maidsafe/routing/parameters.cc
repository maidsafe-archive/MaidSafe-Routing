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

#include "maidsafe/routing/parameters.h"

#include "maidsafe/rudp/parameters.h"
#include "maidsafe/rudp/managed_connections.h"

namespace bptime = boost::posix_time;

namespace maidsafe {

namespace routing {

uint16_t Parameters::thread_count(8);
uint16_t Parameters::num_chunks_to_cache(100);
uint16_t Parameters::closest_nodes_size(8);
uint16_t Parameters::group_size(4);
uint16_t Parameters::proximity_factor(2);
uint16_t Parameters::max_routing_table_size(64);
uint16_t Parameters::routing_table_size_threshold(max_routing_table_size / 4);
uint16_t Parameters::max_routing_table_size_for_client(8);
uint16_t Parameters::max_client_routing_table_size(max_routing_table_size);
uint16_t Parameters::bucket_target_size(1);
std::chrono::steady_clock::duration Parameters::default_response_timeout(std::chrono::seconds(10));
std::chrono::seconds Parameters::find_node_interval(10);
std::chrono::seconds Parameters::recovery_time_lag(5);
std::chrono::seconds Parameters::re_bootstrap_time_lag(10);
std::chrono::seconds Parameters::find_close_node_interval(3);
uint16_t Parameters::find_node_repeats_per_num_requested(3);
uint16_t Parameters::maximum_find_close_node_failures(10);
uint16_t Parameters::max_route_history(5);
uint16_t Parameters::hops_to_live(50);
uint16_t Parameters::accepted_distance_tolerance(1);
uint16_t Parameters::greedy_fraction(Parameters::max_routing_table_size * 3 / 4);
std::chrono::steady_clock::duration Parameters::local_retreival_timeout(std::chrono::seconds(2));
uint16_t Parameters::routing_table_ready_to_response(Parameters::greedy_fraction * 9 / 10);
bptime::time_duration Parameters::connect_rpc_prune_timeout(
    rudp::Parameters::rendezvous_connect_timeout * 2);
// 10 KB of book keeping data for Routing
uint32_t Parameters::max_data_size(rudp::ManagedConnections::kMaxMessageSize() - 10240);
bool Parameters::append_maidsafe_endpoints(false);
// TODO(Prakash): BEFORE_RELEASE revisit below preprocessor directives to remove internal endpoints
#if defined QA_BUILD || defined TESTING
// TODO(Prakash) : Revert append_maidsafe_local_endpoints to true once local network available.
bool Parameters::append_maidsafe_local_endpoints(false);
#else
bool Parameters::append_maidsafe_local_endpoints(false);
#endif
// TODO(Prakash) : To allow bootstrapping off nodes on same machine, revert once local network
// available
bool Parameters::append_local_live_port_endpoint(false);
// TODO(Prakash): BEFORE_RELEASE enable caching after persona tests are passing
bool Parameters::caching(true);
}  // namespace routing

}  // namespace maidsafe
