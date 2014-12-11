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

#ifndef MAIDSAFE_ROUTING_TOOLS_NETWORK_VIEWER_CONTROLLERS_MAIN_VIEW_H_
#define MAIDSAFE_ROUTING_TOOLS_NETWORK_VIEWER_CONTROLLERS_MAIN_VIEW_H_

// std
#include <memory>
#include <string>

#include "helpers/qt_push_headers.h"
#include "ui_main_view.h"  // NOLINT - Viv
#include "helpers/qt_pop_headers.h"

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
