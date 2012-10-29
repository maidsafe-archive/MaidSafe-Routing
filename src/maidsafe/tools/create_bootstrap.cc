/***************************************************************************************************
 *  Copyright 2012 maidsafe.net limited                                                            *
 *                                                                                                 *
 *  The following source code is property of MaidSafe.net limited and is not meant for external    *
 *  use. The use of this code is governed by the licence file licence.txt found in the root of     *
 *  this directory and also on www.maidsafe.net.                                                   *
 *                                                                                                 *
 *  You are not free to copy, amend or otherwise use this source code without the explicit written *
 *  permission of the board of directors of MaidSafe.net.                                          *
 **************************************************************************************************/

#if defined MAIDSAFE_WIN32
#  include <windows.h>
#else
#  include <unistd.h>
#  if defined MAIDSAFE_LINUX
#    include <termio.h>
#  elif defined MAIDSAFE_APPLE
#    include <termios.h>
#  endif
#endif

#include <cstdint>
#include <fstream>
#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <vector>
#include <utility>
#include "boost/filesystem.hpp"
#include "maidsafe/common/utils.h"
#include "maidsafe/common/crypto.h"
#include "maidsafe/common/rsa.h"
#include "maidsafe/routing/routing_pb.h"


namespace fs = boost::filesystem;

typedef std::pair<int, std::string> Endpoint;

static std::string prompt(">> ");
static std::vector<Endpoint> endpoints;

template <class T>
T Get(std::string display_message, bool echo_input = true);


void Echo(bool enable = true) {
#ifdef WIN32
  HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
  DWORD mode;
  GetConsoleMode(hStdin, &mode);

  if (!enable)
    mode &= ~ENABLE_ECHO_INPUT;
  else
    mode |= ENABLE_ECHO_INPUT;

  SetConsoleMode(hStdin, mode);
#else
  struct termios tty;
  tcgetattr(STDIN_FILENO, &tty);
  if (!enable)
    tty.c_lflag &= ~ECHO;
  else
    tty.c_lflag |= ECHO;

  (void) tcsetattr(STDIN_FILENO, TCSANOW, &tty);
#endif
}

void AddEndPoint() {
  std::string ip_address = Get<std::string>("please enter IP addess");
  int port = Get<int>("please enter port");
  endpoints.push_back(std::make_pair(port, ip_address));
}
void ListEndPoints() {
  int count = 1;
  for (auto i = endpoints.begin(); endpoints.end() != i; ++i, ++count) {
    std::cout << " ID: " <<  count << " IP Address : " << (*i).second << " Port : "
      << (*i).first << "\n";
  }
}

void DeleteEndPoint() {
  ListEndPoints();
  size_t id = Get<int>("please enter ID to remove");
  if (id < endpoints.size())
    endpoints.erase(endpoints.begin() + id);
}

void ReadFile() {
  std::string filename = Get<std::string>("please enter filename to load");
  fs::path file(filename);
  maidsafe::routing::protobuf::Bootstrap protobuf_bootstrap;

  std::string serialised_endpoints;
  if (!maidsafe::ReadFile(file, &serialised_endpoints)) {
    std::cout << "Could not read bootstrap file.";
    return;
  }

  if (!protobuf_bootstrap.ParseFromString(serialised_endpoints)) {
    std::cout << "Could not parse bootstrap file.";
    return;
  }
  endpoints.clear();
  endpoints.reserve(protobuf_bootstrap.bootstrap_contacts().size());
  for (int i = 0; i < protobuf_bootstrap.bootstrap_contacts().size(); ++i) {
    endpoints.push_back(std::make_pair(static_cast<int>(
          protobuf_bootstrap.bootstrap_contacts(i).port()),
        protobuf_bootstrap.bootstrap_contacts(i).ip()));
  }
}

void WriteFile() {
  std::string filename = Get<std::string>("please enter filename to write");
  fs::path file(filename);
  maidsafe::routing::protobuf::Bootstrap protobuf_bootstrap;

  for (size_t i = 0; i < endpoints.size(); ++i) {
    maidsafe::routing::protobuf::Endpoint* endpoint = protobuf_bootstrap.add_bootstrap_contacts();
    endpoint->set_ip(endpoints[i].second);
    endpoint->set_port(endpoints[i].first);
  }

  std::string serialised_bootstrap_nodes;
  if (!protobuf_bootstrap.SerializeToString(&serialised_bootstrap_nodes)) {
    std::cout << "Could not serialise bootstrap contacts.";
    return;
  }

  if (!maidsafe::WriteFile(file, serialised_bootstrap_nodes)) {
    std::cout << "Could not write bootstrap file.";
    return;
  }
  return;
}

void exit() {
  exit(0);
}

void Help() { std::cout << "\t\tmaidsafe bootstrap create tool \n"
            << "_________________________________________________________________\n"
            << "1:  add_endpoint   \t \t Add an endpoint                 \n"
            << "2:  list endpoints \t\t list endpoints                  \n"
            << "3:  del_endpoint \t\t remove an endpoint               \n"
            << "4:  write_file  \t\t write to a file                  \n"
            << "5:  read_file  \t\t\t read from  a file              \n"
            << "_________________________________________________________________\n"
            << "0:  exit the system;";
}

void Process(int command) {
  switch (command) {
  case 0:
    exit();
    break;
  case 1:
    AddEndPoint();
    break;
  case 2:
    ListEndPoints();
    break;
  case 3:
    DeleteEndPoint();
    break;
  case 4:
    WriteFile();
    break;
  case 5:
    ReadFile();
    break;
  default :
    std::cout << "unknown option \n";
    std::cout << prompt << std::flush;
    Help();
  }
}

#if defined MAIDSAFE_WIN32
#  pragma warning(push)
#  pragma warning(disable: 4701)
#endif
template <class T>
T Get(std::string display_message, bool echo_input) {
  Echo(echo_input);
  std::cout << display_message << "\n";
  std::cout << prompt << std::flush;
  T command;
  std::string input;
  while (std::getline(std::cin, input, '\n')) {
    std::cout << prompt << std::flush;
    if (std::stringstream(input) >> command) {
      Echo(true);
      return command;
    } else {
      Echo(true);
      std::cout << "invalid option\n";
      std::cout << prompt << std::flush;
    }
  }
  return command;
}
#if defined MAIDSAFE_WIN32
#  pragma warning(pop)
#endif

int main() {
  for (;;) {
  Echo(true);
    std::cout << "_________________________________________________________________\n";
    Help();
    Process(Get<int>("", true));
    std::cout <<"_________________________________________________________________\n";
  }
}
