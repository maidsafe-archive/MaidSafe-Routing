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

#include "boost/filesystem.hpp"

namespace maidsafe {

namespace routing {

struct Parameters {
 public:
  // this node is client or node (full routing node with storage)
  static bool client_mode;
  // fully encrypt all data at routing level in both directions
  static bool encryption_required;
  static std::string &company_name;
  static std::string &application_name;
  // path including filename of config file
  static boost::filesystem::path &bootstrap_file_path;
 private:
  Parameters(const Parameters&);  // no copy
  Parameters& operator=(const Parameters&);  // no assign
};




}  // namespace routing

}  // namespace maidsafe

#endif // MAIDSAFE_ROUTING_PARAMETERS_H_