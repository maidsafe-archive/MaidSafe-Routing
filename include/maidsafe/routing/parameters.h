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


namespace maidsafe {

namespace routing {

struct Parameters {
 public:
  Parameters();
  ~Parameters();
  // fully encrypt all data at routing level in both directions
  static bool encryption_required;
  // Thread count for use of asio::io_service
  static uint16_t thread_count;
  static uint16_t num_chunks_to_cache;
  static uint16_t timeout_in_seconds;
  static uint16_t closest_nodes_size;
  static uint16_t node_group_size;
  static uint16_t max_routing_table_size;
  static uint16_t max_client_routing_table_size;
  static uint16_t max_non_routing_table_size;
  static uint16_t bucket_target_size;
  static uint32_t max_data_size;
  static uint16_t recovery_timeout_in_seconds;
  static uint16_t max_route_history;

 private:
  Parameters(const Parameters&);
  Parameters(const Parameters&&);
  Parameters& operator=(const Parameters&);
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_PARAMETERS_H_
