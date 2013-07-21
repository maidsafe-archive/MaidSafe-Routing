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
