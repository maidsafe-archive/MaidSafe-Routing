/*  Copyright 2012 MaidSafe.net limited

    This MaidSafe Software is licensed to you under (1) the MaidSafe.net Commercial License,
    version 1.0 or later, or (2) The General Public License (GPL), version 3, depending on which
    licence you accepted on initial access to the Software (the "Licences").

    By contributing code to the MaidSafe Software, or to this project generally, you agree to be
    bound by the terms of the MaidSafe Contributor Agreement, version 1.0, found in the root
    directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also
    available at: http://www.novinet.com/license

    Unless required by applicable law or agreed to in writing, the MaidSafe Software distributed
    under the GPL Licence is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS
    OF ANY KIND, either express or implied.

    See the Licences for the specific language governing permissions and limitations relating to
    use of the MaidSafe Software.                                                                 */

#include "maidsafe/routing/tools/network_viewer/models/api_helper.h"

#include <chrono>

#include "maidsafe/routing/tools/network_viewer/helpers/qt_push_headers.h"
#include "maidsafe/routing/tools/network_viewer/helpers/qt_pop_headers.h"

#include "maidsafe/common/tools/network_viewer.h"
#include "maidsafe/common/node_id.h"
#include "maidsafe/common/utils.h"

namespace maidsafe {

APIHelper::APIHelper(QObject* parent) : QObject(parent) {
  network_viewer::SetUpdateFunctor([this](int state_id) { NetworkUpdated(state_id); });
  network_viewer::Run(std::chrono::milliseconds(1000));
}

APIHelper::~APIHelper() {
  network_viewer::Stop();
}

std::vector<std::string> APIHelper::GetNodesInNetwork(int state_id) const {
  qDebug() << QString("APIHelper::GetNodesInNetwork for State: %1")
                  .arg(QString::number(state_id));
  return network_viewer::GetNodesInNetwork(state_id);
}

std::vector<network_viewer::ViewableNode> APIHelper::GetCloseNodes(int state_id,
                                                                   const std::string& id) const {
  qDebug() << QString("APIHelper::GetCloseNodes for State: %1 Node: %2")
                  .arg(QString::number(state_id))
                  .arg(GetShortNodeId(id));
  return network_viewer::GetCloseNodes(state_id, id);
}

void APIHelper::NetworkUpdated(int state_id) {
  qDebug() << QString("APIHelper::NetworkUpdated invoked with State: %1")
                  .arg(QString::number(state_id));
  emit RequestGraphRefresh(state_id);
}

QString APIHelper::GetShortNodeId(std::string node_id) const {
  QString q_node(QString::fromStdString(node_id));
  if (q_node.length() < 13)
    return q_node;
  return q_node.left(6) + "..." + q_node.right(6);
}

}  // namespace maidsafe
