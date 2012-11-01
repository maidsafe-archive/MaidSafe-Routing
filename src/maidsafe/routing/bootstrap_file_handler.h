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
