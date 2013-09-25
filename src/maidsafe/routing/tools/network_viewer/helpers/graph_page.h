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

#ifndef MAIDSAFE_ROUTING_TOOLS_NETWORK_VIEWER_HELPERS_GRAPH_PAGE_H_
#define MAIDSAFE_ROUTING_TOOLS_NETWORK_VIEWER_HELPERS_GRAPH_PAGE_H_

// std
#include <memory>
#include <string>

#include "maidsafe/routing/tools/network_viewer/helpers/qt_push_headers.h"
#include "maidsafe/routing/tools/network_viewer/helpers/qt_pop_headers.h"

#include "maidsafe/routing/tools/network_viewer/models/api_helper.h"

namespace maidsafe {

namespace network_viewer {
struct ViewableNode;
}

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

 private
slots:  // NOLINT - Viv
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
