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

#ifndef MAIDSAFE_ROUTING_TOOLS_NETWORK_VIEWER_HELPERS_GRAPH_PAGE_H_
#define MAIDSAFE_ROUTING_TOOLS_NETWORK_VIEWER_HELPERS_GRAPH_PAGE_H_

// std
#include <memory>
#include <string>

#include "maidsafe/routing/tools/network_viewer/helpers/qt_push_headers.h"
#include "maidsafe/routing/tools/network_viewer/helpers/qt_pop_headers.h"

#include "maidsafe/routing/tools/network_viewer/models/api_helper.h"

namespace maidsafe {

namespace network_viewer { struct ViewableNode; }

class APIHelper;

class GraphPage : public QWebPage {
  Q_OBJECT

 public:
  GraphPage(std::shared_ptr<APIHelper> api_helper, QObject* parent = 0);
  ~GraphPage() {}
  void RenderGraph(int state_id, std::string parent_id, bool is_data_node);

 protected:
  virtual void javaScriptAlert(QWebFrame* frame, const QString& msg);

 signals:
  void RequestNewGraphView(const QString& new_parent_id);

 private slots:  // NOLINT - Viv
  void RefreshGraph(int state_id);
  void RenderNode(int state_id, std::string node_id, bool is_parent, bool is_data_node);

 private:
  typedef network_viewer::ViewableNode Node;
  GraphPage(const GraphPage&);
  GraphPage& operator=(const GraphPage&);
  QString CreateEdge(std::string parent_id, const Node& child_node, QString* current_content);
  QString CreateProximityNode(const Node& child_node, int proximity);
  void SetGraphContents(const QString& entire_content);
  void InitSignals();

  std::shared_ptr<APIHelper> api_helper_;
  QString current_graph_data_;
  std::string current_parent_id_;
  bool is_data_node_;
  QList<std::string> expanded_children_;
};

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_TOOLS_NETWORK_VIEWER_HELPERS_GRAPH_PAGE_H_
