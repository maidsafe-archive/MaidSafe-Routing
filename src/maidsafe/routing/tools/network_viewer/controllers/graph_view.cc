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

#include "maidsafe/routing/tools/network_viewer/controllers/graph_view.h"

#include "maidsafe/routing/tools/network_viewer/helpers/qt_push_headers.h"
#include "maidsafe/routing/tools/network_viewer/helpers/qt_pop_headers.h"

#include "maidsafe/routing/tools/network_viewer/helpers/graph_page.h"
#include "maidsafe/routing/tools/network_viewer/models/api_helper.h"

namespace maidsafe {

GraphViewController::GraphViewController(std::shared_ptr<APIHelper> api_helper, QWidget* parent)
    : QWidget(parent),
      view_(),
      api_helper_(api_helper),
      graph_page_() {
  view_.setupUi(this);
  graph_page_ = new GraphPage(api_helper_, this);
  view_.graph_->setPage(graph_page_);
  InitSignals();
}

void GraphViewController::RenderNode(std::string parent_id, bool is_data_node) {
  graph_page_->RenderGraph(-1, parent_id, is_data_node);
}

void GraphViewController::closeEvent(QCloseEvent* /*event*/) {
  deleteLater();
}

void GraphViewController::InitSignals() {
  connect(graph_page_,        SIGNAL(RequestNewGraphView(const QString&)),    // NOLINT - Viv
          this,               SIGNAL(RequestNewGraphView(const QString&)));   // NOLINT - Viv
}

}  // namespace maidsafe
