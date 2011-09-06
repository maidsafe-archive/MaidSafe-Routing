/* Copyright (c) 2011 maidsafe.net limited
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
#include "boost/asio/io_service.hpp"
#include "boost/thread/thread.hpp"
#include "maidsafe/dht/transport/transport.h"
#include "maidsafe/dht/transport/tcp_transport.h"
#ifdef __MSVC__
#  pragma warning(push)
#  pragma warning(disable: 4127 4244 4267)
#endif
#include "maidsafe/dht/transport/transport.pb.h"
#ifdef __MSVC__
#  pragma warning(pop)
#endif

int main(int /*argc*/, char ** /*argv*/) {
  boost::asio::io_service asio_service;
  std::shared_ptr<boost::asio::io_service::work> work(
      new boost::asio::io_service::work(asio_service));
  boost::thread worker(
      std::bind(static_cast<size_t(boost::asio::io_service::*)()>(
          &boost::asio::io_service::run), std::ref(asio_service)));

  std::shared_ptr<maidsafe::dht::transport::TcpTransport> tcp_transport(
      new maidsafe::dht::transport::TcpTransport(asio_service));

  maidsafe::dht::transport::protobuf::CrashReport crash_report;
//  *crash_report.mutable_content() = assign crash report content

  maidsafe::dht::transport::Endpoint crash_server;  // assign crash server's
                                                    // endpoint - resolve
                                                    // crash.maidsafe.net;

  int result(0);
  try {
    std::string message(crash_report.SerializeAsString());
    tcp_transport->Send(message, crash_server,
                        maidsafe::dht::transport::kDefaultInitialTimeout);
  }
  catch (const std::exception &) {
    result = -1;
  }

  work.reset();
  asio_service.stop();
  worker.join();

  return result;
}
