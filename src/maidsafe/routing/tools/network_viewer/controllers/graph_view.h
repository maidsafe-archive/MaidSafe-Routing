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

#ifndef MAIDSAFE_ROUTING_TOOLS_NETWORK_VIEWER_CONTROLLERS_GRAPH_VIEW_H_
#define MAIDSAFE_ROUTING_TOOLS_NETWORK_VIEWER_CONTROLLERS_GRAPH_VIEW_H_

// std
#include <memory>
#include <string>

#include "maidsafe/routing/tools/network_viewer/helpers/qt_push_headers.h"
#include "ui_graph_view.h"  // NOLINT - Viv
#include "maidsafe/routing/tools/network_viewer/helpers/qt_pop_headers.h"

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
