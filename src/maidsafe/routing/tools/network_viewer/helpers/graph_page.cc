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

#include "helpers/graph_page.h"

#include <limits>
#include <vector>

#include "maidsafe/common/tools/network_viewer.h"

namespace maidsafe {

GraphPage::GraphPage(std::shared_ptr<APIHelper> api_helper, QObject* parent)
    : QWebPage(parent),
      api_helper_(api_helper),
      current_graph_data_(),
      current_parent_id_(),
      is_data_node_(false),
      expanded_children_() {
  QFile frame_template(":/index.html");
  frame_template.open(QFile::ReadOnly | QIODevice::Text);
  mainFrame()->setHtml(QLatin1String(frame_template.readAll()));
}

void GraphPage::RenderGraph(int state_id, std::string parent_id, bool is_data_node) {
  SetGraphContents(QString());
  expanded_children_.clear();
  current_parent_id_ = parent_id;
  is_data_node_ = is_data_node;
  if (parent_id.empty())
    return;
  RenderNode(state_id, parent_id, true, is_data_node);
}

void GraphPage::javaScriptAlert(QWebFrame* /*frame*/, const QString& msg) {
  QStringList message_parts(msg.split("-"));
  if (message_parts.count() < 2)
    return;
  if (message_parts.at(0) == "click") {
    std::string Address(message_parts.at(1).toStdString());
    if (expanded_children_.contains(Address)) {
      expanded_children_.removeOne(Address);
      RefreshGraph(-1);
    } else {
      expanded_children_.push_back(Address);
      RenderNode(-1, Address, false, false);
    }
  } else if (message_parts.at(0) == "dblclick") {
    RenderGraph(-1, message_parts.at(1).toStdString(), false);
  } else if (message_parts.at(0) == "newview") {
    emit RequestNewGraphView(message_parts.at(1));
  }
}

void GraphPage::RefreshGraph(int state_id) {
  RenderNode(state_id, current_parent_id_, true, is_data_node_);
  foreach (std::string Address, expanded_children_) { RenderNode(state_id, node_id, false, false); }
}

void GraphPage::RenderNode(int state_id, std::string Address, bool is_parent, bool is_data_node) {
  QString graph_contents;
  std::vector<Node> children(api_helper_->GetCloseNodes(state_id, Address));
  if (is_parent) {
    graph_contents = QString("%1 {routingNodeType:%2}\\n").arg(QString::fromStdString(Address)).arg(
        is_data_node ? "dataNode" : "mainNode");
  } else if (expanded_children_.contains(Address) &&
             current_graph_data_.contains(
                 QString("%1 {routingDistance:").arg(QString::fromStdString(Address)))) {
    graph_contents = current_graph_data_.replace(
        QString("%1 {routingDistance:").arg(QString::fromStdString(Address)),
        QString("%1 {isExpanded:1, routingDistance:").arg(QString::fromStdString(Address)));
  } else if (expanded_children_.contains(Address) &&
             !current_graph_data_.contains(
                 QString("%1 {isExpanded:").arg(QString::fromStdString(Address)))) {
    graph_contents =
        current_graph_data_ + QString("%1 {isExpanded:1}\\n").arg(QString::fromStdString(Address));
  } else {
    graph_contents = current_graph_data_;
  }

  for (size_t i(0); i < children.size(); ++i) {
    graph_contents.append(CreateEdge(Address, children.at(i), &graph_contents));
    if (is_parent) {
      assert(i < static_cast<uintmax_t>(std::numeric_limits<int>::max()));
      graph_contents.append(CreateProximityNode(children.at(i), static_cast<int>(i) + 1));
    }
  }
  SetGraphContents(graph_contents);
}

QString GraphPage::CreateEdge(std::string parent_id, const Node& child_node,
                              QString* current_content) {
  QString q_parent_id(QString::fromStdString(parent_id));
  QString q_child_id(QString::fromStdString(child_node.id));
  QString q_child_status(QString::number(static_cast<int32_t>(child_node.type)));
  QString possibleFirstEdge(QString("%1 -> %2 {").arg(q_child_id).arg(q_parent_id));
  if (current_content->contains(possibleFirstEdge)) {
    current_content->replace(possibleFirstEdge, possibleFirstEdge + "onlyShowArrow:true, ");
    return QString("%1 -> %2 {nodeStatus:%3, doubleArrow:true}\\n")
        .arg(q_parent_id)
        .arg(q_child_id)
        .arg(q_child_status);
  }
  return QString("%1 -> %2 {nodeStatus:%3}\\n").arg(q_parent_id).arg(q_child_id).arg(
      q_child_status);
}

QString GraphPage::CreateProximityNode(const Node& child_node, int proximity) {
  QString q_child_id(QString::fromStdString(child_node.id));
  QString q_child_distance(QString::fromStdString(child_node.distance));
  return QString("%1 {routingDistance:%2, proximityId:%3}\\n")
      .arg(q_child_id)
      .arg(q_child_distance)
      .arg(QString::number(proximity));
}

void GraphPage::SetGraphContents(const QString& entire_content) {
  if (current_graph_data_ == entire_content)
    return;

  current_graph_data_ = entire_content;
  mainFrame()->evaluateJavaScript(QString("setContent('%1')").arg(entire_content));
}

void GraphPage::InitSignals() {
  connect(api_helper_.get(), SIGNAL(RequestGraphRefresh(int)),  // NOLINT - Viv
          this, SLOT(RefreshGraph(int)),                        // NOLINT - Viv
          Qt::QueuedConnection);
}

}  // namespace maidsafe
