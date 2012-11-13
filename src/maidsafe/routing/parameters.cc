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

#include "maidsafe/routing/parameters.h"

#include "maidsafe/rudp/parameters.h"
#include "maidsafe/rudp/managed_connections.h"

namespace bptime = boost::posix_time;

namespace maidsafe {

namespace routing {

uint16_t Parameters::thread_count(8);
uint16_t Parameters::num_chunks_to_cache(100);
uint16_t Parameters::closest_nodes_size(8);
uint16_t Parameters::node_group_size(4);
uint16_t Parameters::max_routing_table_size(32);
uint16_t Parameters::routing_table_size_threshold(max_routing_table_size / 2);
uint16_t Parameters::max_client_routing_table_size(8);
uint16_t Parameters::max_non_routing_table_size(max_routing_table_size);
uint16_t Parameters::bucket_target_size(1);
bptime::time_duration Parameters::find_node_interval(bptime::seconds(10));
bptime::time_duration Parameters::recovery_time_lag(bptime::seconds(5));
bptime::time_duration Parameters::re_bootstrap_time_lag(bptime::seconds(10));
bptime::time_duration Parameters::find_close_node_interval(bptime::seconds(3));
uint16_t Parameters::maximum_find_close_node_failures(10);
uint16_t Parameters::max_route_history(5);
uint16_t Parameters::hops_to_live(20);
uint16_t Parameters::greedy_fraction(Parameters::max_routing_table_size * 1 / 2);
uint16_t Parameters::ungreedy_fraction(Parameters::max_routing_table_size -
                                       Parameters::greedy_fraction);
bptime::time_duration Parameters::connect_rpc_prune_timeout(
    rudp::Parameters::rendezvous_connect_timeout * 2);
// 10 KB of book keeping data for Routing
uint32_t Parameters::max_data_size(rudp::ManagedConnections::kMaxMessageSize() - 10240);
bool Parameters::append_maidsafe_endpoints(false);
bool Parameters::append_maidsafe_local_endpoints(false);

}  // namespace routing

}  // namespace maidsafe
