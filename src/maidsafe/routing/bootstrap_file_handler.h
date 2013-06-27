/* Copyright 2012 MaidSafe.net limited

This MaidSafe Software is licensed under the MaidSafe.net Commercial License, version 1.0 or later,
and The General Public License (GPL), version 3. By contributing code to this project You agree to
the terms laid out in the MaidSafe Contributor Agreement, version 1.0, found in the root directory
of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also available at:

http://www.novinet.com/license

Unless required by applicable law or agreed to in writing, software distributed under the License is
distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
implied. See the License for the specific language governing permissions and limitations under the
License.
*/

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
