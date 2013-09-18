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

#ifndef MAIDSAFE_ROUTING_BOOTSTRAP_FILE_HANDLER_H_
#define MAIDSAFE_ROUTING_BOOTSTRAP_FILE_HANDLER_H_

#include <vector>

#include "boost/asio/ip/udp.hpp"
#include "boost/filesystem/path.hpp"


namespace maidsafe {

namespace routing {

std::vector<boost::asio::ip::udp::endpoint> ReadBootstrapFile();

bool WriteBootstrapFile(const std::vector<boost::asio::ip::udp::endpoint> &endpoints,
                        const boost::filesystem::path& bootstrap_file_path);

void UpdateBootstrapFile(const boost::asio::ip::udp::endpoint& endpoint, bool remove);

std::vector<boost::asio::ip::udp::endpoint> MaidSafeEndpoints();

// TODO(Prakash) : BEFORE_RELEASE remove using local endpoints
std::vector<boost::asio::ip::udp::endpoint> MaidSafeLocalEndpoints();

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_BOOTSTRAP_FILE_HANDLER_H_
