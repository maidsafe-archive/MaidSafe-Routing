/* Copyright 2012 MaidSafe.net limited

This MaidSafe Software is licensed under the MaidSafe.net Commercial License, version 1.0 or later,
and The General Public License (GPL), version 3. By contributing code to this project You agree to
the terms laid out in the MaidSafe Contributor Agreement, version 1.0, found in the root directory
of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also available at:

http://www.novinet.com/license

Unless required by applicable law or agreed to in writing, software distributed under the License is
distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
implied. See the License for the specific language governing permissions and limitations under the
License.
*/

#ifndef MAIDSAFE_ROUTING_PARAMETERS_H_
#define MAIDSAFE_ROUTING_PARAMETERS_H_

#include <chrono>
#include <cstdint>
#include "boost/date_time/posix_time/posix_time_duration.hpp"


namespace maidsafe {

namespace routing {

struct Parameters {
 public:
  // Thread count for use of asio::io_service
  static uint16_t thread_count;
  static uint16_t num_chunks_to_cache;
  static uint16_t closest_nodes_size;
  static uint16_t node_group_size;
  static uint16_t proximity_factor;
  static uint16_t max_routing_table_size;             // max size of RoutingTable owned by vault
  static uint16_t routing_table_size_threshold;
  static uint16_t max_routing_table_size_for_client;  // max size of RoutingTable owned by client
  static uint16_t max_client_routing_table_size;      // max size of ClientRoutingTable
  static uint16_t bucket_target_size;
  static uint32_t max_data_size;
  static std::chrono::steady_clock::duration default_response_timeout;
  static std::chrono::seconds find_node_interval;
  static std::chrono::seconds recovery_time_lag;
  static boost::posix_time::time_duration re_bootstrap_time_lag;
  static boost::posix_time::time_duration find_close_node_interval;
  static uint16_t find_node_repeats_per_num_requested;
  static uint16_t maximum_find_close_node_failures;
  static uint16_t max_route_history;
  static uint16_t hops_to_live;
  static uint16_t greedy_fraction;
  static uint16_t split_avoidance;
  static uint16_t routing_table_ready_to_response;
  static uint16_t accepted_distance_tolerance;
  static boost::posix_time::time_duration connect_rpc_prune_timeout;
  static bool append_maidsafe_endpoints;
  static bool append_maidsafe_local_endpoints;
  static bool append_local_live_port_endpoint;
  static bool caching;

 private:
  Parameters();
  ~Parameters();
  Parameters(const Parameters&);
  Parameters(const Parameters&&);
  Parameters& operator=(const Parameters&);
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_PARAMETERS_H_
