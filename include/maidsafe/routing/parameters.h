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
  static unsigned int thread_count;
  static unsigned int num_chunks_to_cache;
  static unsigned int closest_nodes_size;
  static unsigned int group_size;
  static unsigned int proximity_factor;
  static unsigned int max_routing_table_size;  // max size of RoutingTable owned by vault
  static unsigned int routing_table_size_threshold;
  static unsigned int max_routing_table_size_for_client;  // max size of RoutingTable in client
  static unsigned int max_client_routing_table_size;      // max size of ClientRoutingTable
  static unsigned int bucket_target_size;
  static uint32_t max_data_size;
  static std::chrono::steady_clock::duration default_response_timeout;
  static std::chrono::seconds find_node_interval;
  static std::chrono::seconds recovery_time_lag;
  static std::chrono::seconds re_bootstrap_time_lag;
  static std::chrono::seconds find_close_node_interval;
  static unsigned int find_node_repeats_per_num_requested;
  static unsigned int maximum_find_close_node_failures;
  static unsigned int max_route_history;
  static unsigned int hops_to_live;
  static unsigned int unidirectional_interest_range;
  static std::chrono::steady_clock::duration local_retreival_timeout;
  static unsigned int routing_table_ready_to_response;
  static unsigned int accepted_distance_tolerance;
  static boost::posix_time::time_duration connect_rpc_prune_timeout;
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
