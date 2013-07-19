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

#include "maidsafe/routing/tools/network_viewer/helpers/qt_push_headers.h"
#include "maidsafe/routing/tools/network_viewer/helpers/qt_pop_headers.h"

#include "maidsafe/common/log.h"

#include "maidsafe/routing/tools/network_viewer/controllers/main_view.h"
#include "maidsafe/routing/tools/network_viewer/helpers/application.h"
#include "maidsafe/routing/tools/network_viewer/models/api_helper.h"


int main(int argc, char *argv[]) {
  maidsafe::log::Logging::Instance().Initialise(argc, argv);

  maidsafe::Application application(argc, argv);
  application.setOrganizationDomain("http://www.maidsafe.net");
  application.setOrganizationName("MaidSafe.net Ltd.");
  application.setApplicationName("Network Viewer");
  application.setApplicationVersion("0.1");
  try {
    auto api_helper(std::make_shared<maidsafe::APIHelper>());
    auto main_view_controller(std::make_shared<maidsafe::MainViewController>(api_helper));
    application.SetErrorHandler(main_view_controller);

    // Set Application Plugin Directory
    application.addLibraryPath(QCoreApplication::applicationDirPath() + "/plugins");
    application.quitOnLastWindowClosed();
    int result = application.exec();
    LOG(kInfo) << "App exiting with result " << result;
    return result;
  } catch(const std::exception &ex) {
    LOG(kError) << "STD Exception Caught: " << ex.what();
    return -1;
  } catch(...) {
    LOG(kError) << "Default Exception Caught";
    return -1;
  }
}
