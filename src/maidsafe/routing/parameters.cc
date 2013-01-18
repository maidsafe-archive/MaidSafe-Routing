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
uint16_t Parameters::max_routing_table_size(64);
uint16_t Parameters::routing_table_size_threshold(max_routing_table_size / 2);
uint16_t Parameters::max_client_routing_table_size(8);
uint16_t Parameters::max_non_routing_table_size(max_routing_table_size);
uint16_t Parameters::bucket_target_size(1);
bptime::time_duration Parameters::default_send_timeout(bptime::seconds(10));
bptime::time_duration Parameters::find_node_interval(bptime::seconds(10));
bptime::time_duration Parameters::recovery_time_lag(bptime::seconds(5));
bptime::time_duration Parameters::re_bootstrap_time_lag(bptime::seconds(10));
bptime::time_duration Parameters::find_close_node_interval(bptime::seconds(3));
uint16_t Parameters::maximum_find_close_node_failures(10);
uint16_t Parameters::max_route_history(5);
uint16_t Parameters::hops_to_live(50);
uint16_t Parameters::accepted_distance_tolerance(1);
uint16_t Parameters::greedy_fraction(Parameters::max_routing_table_size * 3 / 4);
uint16_t Parameters::split_avoidance(4);
uint16_t Parameters::ack_timeout(2);
uint16_t Parameters::max_ack_attempts(3);
uint16_t Parameters::message_age_to_drop(10);
uint16_t Parameters::message_history_cleanup_factor(25);
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
bool Parameters::caching(false);
}  // namespace routing

}  // namespace maidsafe
