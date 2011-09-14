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
#include "boost/filesystem/fstream.hpp"
#include "maidsafe/dht/transport/transport.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/dht/transport/tcp_transport.h"
#ifdef __MSVC__
#  pragma warning(push)
#  pragma warning(disable: 4127 4244 4267)
#endif
#include "maidsafe/dht/transport/transport.pb.h"
#ifdef __MSVC__
#  pragma warning(pop)
#endif

namespace arg = std::placeholders;
namespace maid_dht = maidsafe::dht::transport;


boost::mutex mutex_;

std::string ReturnDateTime() {
  std::ostringstream msg;
  const boost::posix_time::ptime cur_time =
      boost::posix_time::second_clock::local_time();
  boost::posix_time::time_facet *setting =
      new boost::posix_time::time_facet("%d-%m-%Y %H;%M;%S");
  msg.imbue(std::locale(msg.getloc(), setting));
  msg << cur_time;
  return msg.str();
}

void DoOnRequestReceived(const std::string& request,
                         const maid_dht::Info& /*info*/,
                         std::string* response,
                         boost::posix_time::time_duration* /* timeout*/) {
  *response = "Received";
  std::cout << "New Error Log Received." << std::endl;
  maidsafe::dht::transport::protobuf::CrashReport crash_report;
  crash_report.ParseFromString(request);
  boost::mutex::scoped_lock lock(mutex_);
  std::cout << "Project Name: " << crash_report.project_name() << std::endl;
  std::cout << "Project Ver: " << crash_report.project_version() << std::endl;
  fs::path out_dir("./Error-Logs/" + crash_report.project_name() +
                   "/" + crash_report.project_version() + "/" +
                   ReturnDateTime() + " - " +
                   maidsafe::RandomAlphaNumericString(3));
  boost::filesystem::create_directory("./Error-Logs/" +
                                      crash_report.project_name());
  boost::filesystem::create_directory("./Error-Logs/" +
                                      crash_report.project_name() +
                                      "/" + crash_report.project_version());
  boost::filesystem::create_directory(out_dir);
  maidsafe::WriteFile(out_dir.string() + "/Error-Log.dmp",
      crash_report.content());
  std::cout << "Waiting for New Log..." << std::endl;
}

void DoOnError(const maidsafe::dht::transport::TransportCondition &tc) {
  std::cout << "Got error " << tc << std::endl;
}

int main(/*int argc, char ** argv*/) {
  int result(0);
  boost::asio::io_service asio_service;
  std::shared_ptr<boost::asio::io_service::work> work(
      new boost::asio::io_service::work(asio_service));
  boost::thread worker(
      std::bind(static_cast<size_t(boost::asio::io_service::*)()>(
          &boost::asio::io_service::run), std::ref(asio_service)));
  boost::thread worker2(
      std::bind(static_cast<size_t(boost::asio::io_service::*)()>(
          &boost::asio::io_service::run), std::ref(asio_service)));
  std::shared_ptr<maid_dht::TcpTransport> tcp_transport(
      new maid_dht::TcpTransport(asio_service));
  tcp_transport->on_message_received()->connect(
      std::bind(&DoOnRequestReceived, arg::_1, arg::_2, arg::_3, arg::_4));
  tcp_transport->on_error()->connect(std::bind(&DoOnError, arg::_1));
  if (tcp_transport->StartListening(maid_dht::Endpoint(
                                    "127.0.0.1", 5000)) != 0) {
    std::cout << "Error Starting to Listen" << std::endl;
  }
  std::cout << "Server Started... " << std::endl;
//  maidsafe::Sleep(boost::posix_time::seconds(300));
  while (true) {}
//  Need some sort of loop to hold the exit till work finishes
  work.reset();
  asio_service.stop();
  worker.join();
  return result;
}
