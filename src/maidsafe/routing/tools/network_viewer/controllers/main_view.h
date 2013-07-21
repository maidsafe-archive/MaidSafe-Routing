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

#ifndef MAIDSAFE_ROUTING_TOOLS_NETWORK_VIEWER_CONTROLLERS_MAIN_VIEW_H_
#define MAIDSAFE_ROUTING_TOOLS_NETWORK_VIEWER_CONTROLLERS_MAIN_VIEW_H_

// std
#include <memory>
#include <string>

#include "maidsafe/routing/tools/network_viewer/helpers/qt_push_headers.h"
#include "ui_main_view.h"  // NOLINT - Viv
#include "maidsafe/routing/tools/network_viewer/helpers/qt_pop_headers.h"

namespace maidsafe {

class APIHelper;
class GraphViewController;
class GraphPage;

class MainViewController : public QWidget {
  Q_OBJECT

 public:
  MainViewController(std::shared_ptr<APIHelper> api_helper, QWidget* parent = 0);
  ~MainViewController() {}

 protected:
  bool eventFilter(QObject* object, QEvent* event);

 private slots:  // NOLINT - Viv
  void EventLoopStarted();
  void RefreshRequested(int state_id);
  void SelectionChanged();
  void FilterChanged(const QString& new_filter);
  void NewGraphViewRequested(const QString& new_parent_id);
  void OpenDataViewer();

 private:
  MainViewController(const MainViewController&);
  MainViewController& operator=(const MainViewController&);
  void PopulateNodes();
  void CreateGraphController(const QString& new_parent_id, bool is_data_node);
  void InitSignals();

  Ui::MainView view_;
  std::shared_ptr<APIHelper> api_helper_;
  GraphPage* main_page_;
  int last_network_state_id_;
};

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_TOOLS_NETWORK_VIEWER_CONTROLLERS_MAIN_VIEW_H_
