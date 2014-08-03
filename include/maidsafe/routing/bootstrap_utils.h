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

#ifndef MAIDSAFE_ROUTING_BOOTSTRAP_UTILS_H_
#define MAIDSAFE_ROUTING_BOOTSTRAP_UTILS_H_

#ifdef _MSC_VER
#include <mutex>
#endif
#include <vector>

#include "boost/asio/ip/udp.hpp"
#include "boost/filesystem/path.hpp"
#include "boost/filesystem/operations.hpp"


#include "maidsafe/common/log.h"
#include "maidsafe/routing/api_config.h"
#include "maidsafe/routing/bootstrap_file_operations.h"

namespace maidsafe {

namespace routing {

namespace detail {

template <bool is_client>
boost::filesystem::path GetCurrentBootstrapFilePath() {
  auto path_getter([] {
    return boost::filesystem::exists(GetOverrideBootstrapFilePath<is_client>())
               ? GetOverrideBootstrapFilePath<is_client>()
               : GetDefaultBootstrapFilePath<is_client>();
  });
#ifdef _MSC_VER
  static std::once_flag initialised_flag;
  static boost::filesystem::path kCurrentBootstrapFilePath;
  std::call_once(initialised_flag, [&] { kCurrentBootstrapFilePath = path_getter(); });
#else
  static const boost::filesystem::path kCurrentBootstrapFilePath{path_getter()};
#endif
  return kCurrentBootstrapFilePath;
}

}  // namespace detail

BootstrapContacts GetBootstrapContacts(bool is_client = true);

BootstrapContacts GetZeroStateBootstrapContacts(boost::asio::ip::udp::endpoint local_endpoint);

void InsertOrUpdateBootstrapContact(const BootstrapContact& bootstrap_contact, bool is_client);


}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_BOOTSTRAP_UTILS_H_