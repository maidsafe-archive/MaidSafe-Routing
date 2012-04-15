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

#ifndef MAIDSAFE_ROUTING_ERROR_H_
#define MAIDSAFE_ROUTING_ERROR_H_

#include <system_error>

namespace maidsafe {

namespace routing {

enum class routing_error {
  // General
  Ok = 10,
  timed_out = 100,
  // RoutingTable
  own_id_not_includable = 200,
  failed_to_insert_new_contact = 300,
  failed_to_find_contact = 400,
  failed_to_set_publicKey = 500,
  failed_to_update_rank = 600,
  failed_to_set_preferred_endpoint = 700,
  // Node
  no_online_bootstrap_contacts = 800,
  invalid_bootstrap_contacts = 900,
  not_joined = 1000,
  cannot_write_config = 1100
};

enum class routing_error_condition {
  routing_table_error = 100,
  node_error = 200,
  file_error = 300,
  network_error = 400
};

class routing_category_impl : public std::error_category {
 public:
    virtual const char* name() const;
    virtual std::string message(int ev) const;
    virtual std::error_condition default_error_condition(int ev) const;
};

std::error_code make_error_code(routing_error e);
std::error_condition make_error_condition(routing_error e);

const std::error_category &routing_category();

}  // namespace routing

} // namespace maidsafe

#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Weffc++"
#endif
namespace std
{
  template <>
  struct is_error_code_enum<maidsafe::routing::routing_error>
    : public true_type {};
}
#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif

#endif  // MAIDSAFE_ROUTING_ERROR_H_

