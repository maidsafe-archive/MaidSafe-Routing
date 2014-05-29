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

#if defined MAIDSAFE_WIN32
#include <windows.h>
#else
#include <unistd.h>

#include <termios.h>



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

#include "maidsafe/routing/bootstrap_file_operations.h"

namespace fs = boost::filesystem;

typedef std::pair<int, std::string> Endpoint;

static std::string prompt(">> ");
static maidsafe::routing::BootstrapContacts bootstrap_contacts;

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

  (void)tcsetattr(STDIN_FILENO, TCSANOW, &tty);
#endif
}

void AddEndPoint() {
  std::string ip_address = Get<std::string>("please enter IP addess");
  uint16_t port = Get<uint16_t>("please enter port");
  boost::asio::ip::udp::endpoint endpoint(boost::asio::ip::address::from_string(ip_address), port);
  bootstrap_contacts.push_back(endpoint);
}
void ListEndPoints() {
  int count = 1;
  for (auto i = bootstrap_contacts.begin(); bootstrap_contacts.end() != i; ++i, ++count) {
    std::cout << " ID: " << count << " IP Address : " << (*i).address() << " Port : " << (*i).port()
              << "\n";
  }
}

void DeleteEndPoint() {
  ListEndPoints();
  size_t id = Get<int>("please enter ID to remove");
  if (id < bootstrap_contacts.size())
    bootstrap_contacts.erase(bootstrap_contacts.begin() + id);
}

void ReadFile() {
  std::string filename = Get<std::string>("please enter filename to load");
  fs::path file(filename);
  try {
    bootstrap_contacts = maidsafe::routing::ReadBootstrapFile(file);
  }  catch (const std::exception& e) {
    std::cout << "Could not read bootstrap file. Error : " << e.what();;
    return;
  }
}

void WriteFile() {
  std::string filename = Get<std::string>("please enter filename to write");
  fs::path file(filename);

  try {
    maidsafe::routing::WriteBootstrapFile(bootstrap_contacts, file);
  }  catch (const std::exception& e) {
    std::cout << "Could not write bootstrap file. Error : " << e.what();;
    return;
  }
}

void exit() { exit(0); }

void Help() {
  std::cout << "\t\tmaidsafe bootstrap create tool \n"
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
    default:
      std::cout << "unknown option \n";
      std::cout << prompt << std::flush;
      Help();
  }
}

#if defined MAIDSAFE_WIN32
#pragma warning(push)
#pragma warning(disable : 4701)
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
#pragma warning(pop)
#endif

int main() {
  for (;;) {
    Echo(true);
    std::cout << "_________________________________________________________________\n";
    Help();
    Process(Get<int>("", true));
    std::cout << "_________________________________________________________________\n";
  }
}
