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

#ifndef MAIDSAFE_ROUTING_TOOLS_NETWORK_VIEWER_CONTROLLERS_GRAPH_VIEW_H_
#define MAIDSAFE_ROUTING_TOOLS_NETWORK_VIEWER_CONTROLLERS_GRAPH_VIEW_H_

// std
#include <memory>
#include <string>

#include "helpers/qt_push_headers.h"
#include "ui_graph_view.h"  // NOLINT - Viv
#include "helpers/qt_pop_headers.h"

namespace maidsafe {

class APIHelper;
class GraphPage;

class GraphViewController : public QWidget {
  Q_OBJECT

 public:
  explicit GraphViewController(std::shared_ptr<APIHelper> api_helper, QWidget* parent = 0);
  ~GraphViewController() {}
  void RenderNode(std::string parent_id, bool is_data_node);

 protected:
  virtual void closeEvent(QCloseEvent* event);

 signals:
  void RequestNewGraphView(const QString& new_parent_id);

 private:
  GraphViewController(const GraphViewController&);
  GraphViewController& operator=(const GraphViewController&);
  void InitSignals();

  Ui::GraphView view_;
  std::shared_ptr<APIHelper> api_helper_;
  GraphPage* graph_page_;
};

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_TOOLS_NETWORK_VIEWER_CONTROLLERS_GRAPH_VIEW_H_
