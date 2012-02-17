// /* Copyright (c) 2010 maidsafe.net limited
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
// 
//     * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//     * Neither the name of the maidsafe.net limited nor the names of its
//     contributors may be used to endorse or promote products derived from this
//     software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
// TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
// THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// */
// 
 #ifndef MAIDSAFE_ROUTING_CONFIG_H_
 #define MAIDSAFE_ROUTING_CONFIG_H_
// 
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "boost/asio/ip/address.hpp"
#include "boost/asio/io_service.hpp"
#include "boost/date_time/posix_time/posix_time_duration.hpp"
#include "boost/signals2/signal.hpp"

#include "maidsafe/common/rsa.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/version.h"

#if MAIDSAFE_ROUTING_VERSION != 3107
#  error This API is not compatible with the installed library.\
    Please update the maidsafe-routing library.
#endif
// 
// 
 namespace maidsafe {
// 
// namespace transport {
// class Transport;
// struct Endpoint;
// struct Info;
// }  // namespace transport
// 
 namespace routing {
// 
// 
// class NodeId;
// 
// enum OnlineStatus { kOffline, kOnline, kAttemptingConnect };
// 
// typedef std::shared_ptr<boost::signals2::signal<void(OnlineStatus)>>
//         OnOnlineStatusChangePtr;
// 
// 
// typedef NodeId Key;
// typedef boost::asio::ip::address IP;
// typedef uint16_t Port;
// 
// typedef boost::asio::io_service AsioService;
// typedef std::shared_ptr<MessageHandler> MessageHandlerPtr;
// typedef std::shared_ptr<transport::Transport> TransportPtr;
// typedef std::shared_ptr<asymm::Keys> KeyPairPtr;
// typedef std::shared_ptr<asymm::PrivateKey> PrivateKeyPtr;
// typedef std::shared_ptr<asymm::PublicKey> PublicKeyPtr;
// typedef std::shared_ptr<transport::Info> RankInfoPtr;
// 
// 
// // The size of ROUTING keys and node IDs in bytes.
//  const uint16_t kKeySizeBytes(64);
// 
// 
// 
// // The minimum number of directly-connected contacts returned by
// // GetBootstrapContacts.  If there are less than this, the list has all other
// // known contacts appended.
// const uint16_t kMinBootstrapContacts(8);
// 
}  // namespace routing
 
}  // namespace maidsafe
// 
#endif  // MAIDSAFE_ROUTING_CONFIG_H_
