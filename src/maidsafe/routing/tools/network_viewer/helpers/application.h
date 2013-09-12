/*  Copyright 2012 MaidSafe.net limited

    This MaidSafe Software is licensed to you under (1) the MaidSafe.net Commercial License,
    version 1.0 or later, or (2) The General Public License (GPL), version 3, depending on which
    licence you accepted on initial access to the Software (the "Licences").

    By contributing code to the MaidSafe Software, or to this project generally, you agree to be
    bound by the terms of the MaidSafe Contributor Agreement, version 1.0, found in the root
    directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also
    available at: http://www.novinet.com/license

    Unless required by applicable law or agreed to in writing, the MaidSafe Software distributed
    under the GPL Licence is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS
    OF ANY KIND, either express or implied.

    See the Licences for the specific language governing permissions and limitations relating to
    use of the MaidSafe Software.                                                                 */

#ifndef MAIDSAFE_ROUTING_TOOLS_NETWORK_VIEWER_HELPERS_APPLICATION_H_
#define MAIDSAFE_ROUTING_TOOLS_NETWORK_VIEWER_HELPERS_APPLICATION_H_

// std
#include <memory>
#include <string>

#include "maidsafe/routing/tools/network_viewer/helpers/qt_push_headers.h"
#include "maidsafe/routing/tools/network_viewer/helpers/qt_pop_headers.h"

namespace maidsafe {

class MainViewController;

class ExceptionEvent : public QEvent {
 public:
  ExceptionEvent(const QString& exception_message, Type type = QEvent::User);
  ~ExceptionEvent() {}
  QString ExceptionMessage();

 private:
  ExceptionEvent(const ExceptionEvent&);
  ExceptionEvent& operator=(const ExceptionEvent&);

  QString exception_message_;
};

class Application : public QApplication {
 public:
  Application(int& argc, char** argv);
  virtual ~Application() {}
  virtual bool notify(QObject* receiver, QEvent* event);
  void SetErrorHandler(std::weak_ptr<MainViewController> handler_object);

 private:
  Application(const Application&);
  Application& operator=(const Application&);

  std::weak_ptr<MainViewController> handler_object_;
};

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_TOOLS_NETWORK_VIEWER_HELPERS_APPLICATION_H_
