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
#include "boost/thread/condition.hpp"
#include "boost/filesystem/fstream.hpp"
#include "maidsafe/transport/transport.h"
#include "maidsafe/transport/tcp_transport.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/common/log.h"
#ifdef __MSVC__
#  pragma warning(push)
#  pragma warning(disable: 4127 4244 4267)
#endif
#include "maidsafe/dht/crash_reporter/crash.pb.h"
#ifdef __MSVC__
#  pragma warning(pop)
#endif
#include "maidsafe/dht/version.h"

const std::string kServerHost("breakpad.maidsafe.net");
const std::string kServerPort("50000");

namespace arg = std::placeholders;
namespace mt = maidsafe::transport;

void DoOnReplyReceived(const std::string& request,
                       const mt::Info& /*info*/,
                       std::string* /*response*/,
                       boost::posix_time::time_duration* /* timeout*/,
                       boost::mutex* cur_mutex,
                       boost::condition_variable* cv,
                       std::string* reply) {
  boost::mutex::scoped_lock lock(*cur_mutex);
  *reply = request;
  cv->notify_one();
}

int main(int argc, char ** argv) {
  int result(0);
  google::InitGoogleLogging(argv[0]);
  FLAGS_logtostderr = false;
  FLAGS_minloglevel = google::INFO;
  boost::asio::io_service asio_service;
  std::shared_ptr<boost::asio::io_service::work> work(
      new boost::asio::io_service::work(asio_service));
  boost::thread worker(
      std::bind(static_cast<size_t(boost::asio::io_service::*)()>(
          &boost::asio::io_service::run), std::ref(asio_service)));
  std::shared_ptr<mt::TcpTransport> tcp_transport(
      new mt::TcpTransport(asio_service));
  maidsafe::crash_report::protobuf::CrashReport crash_report;
  boost::asio::ip::tcp::resolver endpoint_resolver(asio_service);
  boost::asio::ip::tcp::resolver::query endpoint_query(
      boost::asio::ip::tcp::v4(), kServerHost, kServerPort);
  boost::asio::ip::tcp::resolver::iterator endpoint_iterator =
      endpoint_resolver.resolve(endpoint_query);
  boost::asio::ip::tcp::endpoint server_detail = *endpoint_iterator;
  mt::Endpoint crash_server(server_detail.address(),
                                  server_detail.port());
  try {
    boost::mutex cur_mutex;
    boost::condition_variable cv;
    std::string reply;
    tcp_transport->on_message_received()->connect(
        std::bind(&DoOnReplyReceived, arg::_1, arg::_2, arg::_3, arg::_4,
                  &cur_mutex, &cv, &reply));
    if (argc != 4) {
      throw std::logic_error(std::string("Missing Dump File Info"));
    }
    std::string dump_file_data;
    if (!fs::is_regular_file(fs::path(argv[1]))) {
      throw std::logic_error(std::string("No Dump File Located"));
    }
    maidsafe::ReadFile(fs::path(argv[1]), &dump_file_data);
    crash_report.set_project_name(argv[2]);
    crash_report.set_project_version(argv[3]);
    crash_report.set_content(dump_file_data);
    std::string message(crash_report.SerializeAsString());
    tcp_transport->Send(message,
                        crash_server,
                        mt::kDefaultInitialTimeout);
    boost::mutex::scoped_lock lock(cur_mutex);
    while (reply.empty())
      cv.wait(lock);
  }
  catch(const std::exception &e) {
    LOG(WARNING) << "Error Sending Out Report to Crash Server" << std::endl
                 << e.what();
    result = -1;
  }
  work.reset();
  asio_service.stop();
  worker.join();
  return result;
}
