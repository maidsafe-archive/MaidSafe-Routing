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

namespace maidsafe {

namespace routing {

bool Parameters::client_mode(false);
bool Parameters::encryption_required(false);
boost::filesystem::path Parameters::bootstrap_file_path("bootstrap.endpoints");
std::string Parameters::company_name("MaidSafe");
std::string Parameters::application_name("Routing");

}  // namespace routing

}  // namespace maidsafe
