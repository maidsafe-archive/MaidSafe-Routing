/*******************************************************************************
 *  Copyright 2012 maidsafe.net limited                                        *
 *                                                                             *
 *  The following source code is property of maidsafe.net limited and is not   *
 *  meant for external use.  The use of this code is governed by the licence   *
 *  file licence.txt found in the root of this directory and also on           *
 *  www.maidsafe.net.                                                          *
 *                                                                             *
 *  You are not free to copy, amend or otherwise use this source code without  *
 *  the explicit written permission of the board of directors of maidsafe.net. *
 ******************************************************************************/

#ifndef MAIDSAFE_ROUTING_PARAMETERS_H_
#define MAIDSAFE_ROUTING_PARAMETERS_H_

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
  static uint16_t max_routing_table_size;
  static uint16_t routing_table_size_threshold;
  static uint16_t max_client_routing_table_size;
  static uint16_t max_non_routing_table_size;
  static uint16_t bucket_target_size;
  static uint32_t max_data_size;
  static boost::posix_time::time_duration find_node_interval;
  static boost::posix_time::time_duration recovery_time_lag;
  static boost::posix_time::time_duration re_bootstrap_time_lag;
  static boost::posix_time::time_duration find_close_node_interval;
  static uint16_t maximum_find_close_node_failures;
  static uint16_t max_route_history;
  static uint16_t hops_to_live;
  static boost::posix_time::time_duration connect_rpc_prune_timeout;
  static bool append_maidsafe_endpoints;
  static bool append_maidsafe_local_endpoints;

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
