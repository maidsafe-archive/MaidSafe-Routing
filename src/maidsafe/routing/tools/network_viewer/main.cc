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

#include "maidsafe/routing/tools/network_viewer/helpers/qt_push_headers.h"
#include "maidsafe/routing/tools/network_viewer/helpers/qt_pop_headers.h"

#include "maidsafe/common/log.h"

#include "maidsafe/routing/tools/network_viewer/controllers/main_view.h"
#include "maidsafe/routing/tools/network_viewer/helpers/application.h"
#include "maidsafe/routing/tools/network_viewer/models/api_helper.h"

int main(int argc, char* argv[]) {
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
  }
  catch (const std::exception& ex) {
    LOG(kError) << "STD Exception Caught: " << ex.what();
    return -1;
  }
  catch (...) {
    LOG(kError) << "Default Exception Caught";
    return -1;
  }
}
