/* Copyright (c) 2009 maidsafe.net limited
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
    * Neither the name of the maidsafe.net limited nor the names of its
    contributors may be used to endorse or promote products derived from this
    software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <memory>
#include "boost/thread/locks.hpp"
#include "boost/asio/io_service.hpp"

#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/maidsafe_routing.h"
#include "maidsafe/routing/log.h"

#include "maidsafe/transport/rudp_transport.h"
#include "maidsafe/transport/transport.h"
#include "maidsafe/transport/utils.h"


#include "maidsafe/common/utils.h"


namespace maidsafe {
namespace routing {
  
  typedef protobuf::Contact Contact;
class RoutingImpl {
 public:
   RoutingImpl();
   transport::Endpoint GetLocalEndpoint();
   RoutingTable routing_table_;
   transport::RudpTransport transport_;
   typename T trans_;
   Contact my_contact_;
   
};

RoutingImpl::RoutingImpl() : routing_table_(my_contact_)
{

}

Routing::Routing() :  pimpl_(new RoutingImpl())  {}


transport::Endpoint RoutingImpl::GetLocalEndpoint() {

}
// TODO FIXME - do we even need a listening port at all ?? 
// TODO read in boostrap node list and get id / keys etc.

void Routing::Start(boost::asio::io_service& service) { // NOLINT
  pimpl_->transport_ = (transport::RudpTransport(service));
  std::vector<IP> local_ips(transport::GetLocalAddresses());
  transport::Port  port = RandomInt32() % 1600 + 30000;
// TODO we must only listen on the correct local port
  // this is a very old issue.
  bool breakme(false);
  for (uint16_t i = port; i < 35000; ++i) {
    for (auto it = local_ips.begin(); it != local_ips.end(); ++it) {
      transport::Endpoint ep;
      ep.ip = *it;
      ep.port = i;
      if (pimpl_->transport_.StartListening(ep) == transport::kSuccess) {
        // TODO check we can get to at least a bootsrap node !!! then we
        // have the correct ep
//         if (send and recieve)  // maybe connect is enough !!
//          break; ou of both loops - set a var
            breakme = true;
//         else
//           pimpl_->transport_.StopListening();
      }
      if (breakme)
        break;
    }
  }
  
  pimpl_->routing_table_ = RoutingTable(pimpl_->my_contact_);
}



void Routing::Send(const Message &message) { // NOLINT, cause I dont want pointers
//   Impl->routing_table_ 
}




// TODO get messages from transport




}  // namespace routing
}  // namespace maidsafe