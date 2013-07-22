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

#include "maidsafe/routing/tools/network_viewer/helpers/application.h"

#include "maidsafe/routing/tools/network_viewer/controllers/main_view.h"

namespace maidsafe {

ExceptionEvent::ExceptionEvent(const QString& exception_message, Type type)
    : QEvent(type),
      exception_message_(exception_message) {}

QString ExceptionEvent::ExceptionMessage() {
  return exception_message_;
}

Application::Application(int& argc, char** argv)
    : QApplication(argc, argv),
      handler_object_() {}

bool Application::notify(QObject* receiver, QEvent* event) {
  try {
    return QApplication::notify(receiver, event);
  } catch(std::exception& ex) {
    if (auto handler = handler_object_.lock()) {
      QApplication::instance()->postEvent(handler.get(),
                                          new ExceptionEvent(ex.what()));
    } else {
      QApplication::quit();
    }
  } catch(...) {
    if (auto handler = handler_object_.lock()) {
      QApplication::instance()->postEvent(handler.get(),
                                          new ExceptionEvent(tr("Unknown Exception")));
    } else {
      QApplication::quit();
    }
  }
  return false;
}

void Application::SetErrorHandler(std::weak_ptr<MainViewController> handler_object) {
  if (auto handler = handler_object.lock()) {
    handler_object_ = handler;
  }
}

}  // namespace maidsafe
