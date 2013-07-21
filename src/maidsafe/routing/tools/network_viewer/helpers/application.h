/* Copyright 2012 MaidSafe.net limited

This MaidSafe Software is licensed under the MaidSafe.net Commercial License, version 1.0 or later,
and The General Public License (GPL), version 3. By contributing code to this project You agree to
the terms laid out in the MaidSafe Contributor Agreement, version 1.0, found in the root directory
of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also available at:

http://www.novinet.com/license

Unless required by applicable law or agreed to in writing, software distributed under the License is
distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
implied. See the License for the specific language governing permissions and limitations under the
License.
*/

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
