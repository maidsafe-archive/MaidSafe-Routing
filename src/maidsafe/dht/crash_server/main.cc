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

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <memory>
#include "boost/asio/io_service.hpp"
#include "boost/thread/thread.hpp"
#include "boost/filesystem/fstream.hpp"
#include "maidsafe/dht/transport/transport.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/common/log.h"
#include "maidsafe/dht/transport/tcp_transport.h"
#ifdef __MSVC__
#  pragma warning(push)
#  pragma warning(disable: 4127 4244 4267)
#endif
#include "maidsafe/dht/transport/transport.pb.h"
#ifdef __MSVC__
#  pragma warning(pop)
#endif

const std::string kServerHost("breakpad.maidsafe.net");
const std::string kServerPort("50000");
const std::string kPathToErrorLogs = "/home/viv/Crash-Reports/";
const std::string kPathToCrashInfo = "/home/viv/BreakpadServer-CrashInfo/";

namespace arg = std::placeholders;
namespace maid_dht = maidsafe::dht::transport;


boost::mutex mutex_;

void InitialiseDeamon() {
  /* Our process ID and Session ID */
  pid_t pid, sid;

  /* Fork off the parent process */
  pid = fork();
  if (pid < 0) {
          exit(EXIT_FAILURE);
  }
  /* If we got a good PID, then
     we can exit the parent process. */
  if (pid > 0) {
          exit(EXIT_SUCCESS);
  }

  /* Change the file mode mask */
  umask(0);

  /* Create a new SID for the child process */
  sid = setsid();
  if (sid < 0) {
          LOG(ERROR) << "Error Retrieving Session Details For Deamon";
          exit(EXIT_FAILURE);
  }

  /* Change the current working directory */
  if ((chdir("/")) < 0) {
          LOG(ERROR) << "Error Setting Working Directory For Deamon";
          exit(EXIT_FAILURE);
  }

  /* Close out the standard file descriptors */
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
}

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

bool ProcessMiniDump(const std::string &stackwalk_path,
                     const std::string &minidump_path,
                     const std::string &symbolsfol_path,
                     const std::string &outputfile_path) {
  std::string command = stackwalk_path + " \"" + minidump_path + "\" " +
                        symbolsfol_path + " > \"" + outputfile_path + "\"";
  return (system(command.c_str()) == 0);
}

void DoOnRequestReceived(const std::string& request,
                         const maid_dht::Info& /*info*/,
                         std::string* response,
                         boost::posix_time::time_duration* /* timeout*/) {
  *response = "Received";
  LOG(INFO) << "New Error Log Received.";
  maidsafe::dht::transport::protobuf::CrashReport crash_report;
  crash_report.ParseFromString(request);
  boost::mutex::scoped_lock lock(mutex_);
  LOG(INFO) << "Project Name: " << crash_report.project_name();
  LOG(INFO) << "Project Ver: " << crash_report.project_version();
  fs::path out_dir(kPathToErrorLogs + crash_report.project_name() +
                   "/" + crash_report.project_version() + "/" +
                   ReturnDateTime() + " - " +
                   maidsafe::RandomAlphaNumericString(3));
  boost::filesystem::create_directory(kPathToErrorLogs +
                                      crash_report.project_name());
  boost::filesystem::create_directory(kPathToErrorLogs +
                                      crash_report.project_name() +
                                      "/" + crash_report.project_version());
  boost::filesystem::create_directory(out_dir);
  maidsafe::WriteFile(out_dir.string() + "/Error-Log.dmp",
                      crash_report.content());
  std::string stackwalkfile(kPathToErrorLogs + "minidump_stackwalk");
  std::string dumpfile = out_dir.string() + "/Error-Log.dmp";
  std::string symbolsfile = kPathToErrorLogs + "symbols";
  std::string outfile = out_dir.string() + "/Output.txt";
  if (ProcessMiniDump(stackwalkfile, dumpfile, symbolsfile, outfile)) {
    LOG(INFO) << "Processing Completed Succesfully.";
  } else {
    LOG(INFO) << "Processing Log File via minidump_stackwalk Unsuccessful.";
  }
  LOG(INFO) << "Waiting for New Log...";
}

void DoOnError(const maidsafe::dht::transport::TransportCondition &tc) {
  LOG(WARNING) << "Error Receiving Log: " << tc;
}

int main(int /*argc*/, char ** argv) {
  InitialiseDeamon();
  int result(0);
  google::InitGoogleLogging(argv[0]);
  FLAGS_logtostderr = false;
  FLAGS_minloglevel = google::INFO;
  FLAGS_log_dir = kPathToCrashInfo;
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
  boost::asio::ip::tcp::resolver endpoint_resolver(asio_service);
  boost::asio::ip::tcp::resolver::query endpoint_query(
      boost::asio::ip::tcp::v4(), kServerHost, kServerPort);
  boost::asio::ip::tcp::resolver::iterator endpoint_iterator =
      endpoint_resolver.resolve(endpoint_query);
  boost::asio::ip::tcp::endpoint server_detail = *endpoint_iterator;
  if (tcp_transport->StartListening(
          maid_dht::Endpoint(server_detail.address(),
                             server_detail.port())) != 0) {
    LOG(ERROR) << "Error Starting to Listen";
  } else {
    LOG(INFO) << "Server Started... ";
  }
  while (true) {
    maidsafe::Sleep(boost::posix_time::seconds(300));
  }
  work.reset();
  asio_service.stop();
  worker.join();
  return result;
}
