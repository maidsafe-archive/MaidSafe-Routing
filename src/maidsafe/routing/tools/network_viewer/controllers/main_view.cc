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

#include "maidsafe/routing/tools/network_viewer/controllers/main_view.h"

#include <vector>

#include "QMessageBox"

#include "maidsafe/routing/tools/network_viewer/helpers/qt_push_headers.h"
#include "maidsafe/routing/tools/network_viewer/helpers/qt_pop_headers.h"

#include "maidsafe/routing/tools/network_viewer/controllers/graph_view.h"
#include "maidsafe/routing/tools/network_viewer/helpers/application.h"
#include "maidsafe/routing/tools/network_viewer/helpers/graph_page.h"
#include "maidsafe/routing/tools/network_viewer/models/api_helper.h"

namespace maidsafe {

MainViewController::MainViewController(std::shared_ptr<APIHelper> api_helper, QWidget* parent)
    : QWidget(parent),
      view_(),
      api_helper_(api_helper),
      main_page_(),
      last_network_state_id_(-1) {
  view_.setupUi(this);
  main_page_ = new GraphPage(api_helper_, this);
  view_.dock_->setPage(main_page_);
  view_.tabs_->setCurrentIndex(0);
  InitSignals();
  view_.nodes_->clear();
  view_.count_->setText("0");
  QTimer::singleShot(0, this, SLOT(EventLoopStarted()));
}

bool MainViewController::eventFilter(QObject* object, QEvent* event) {
  if (object == this && event->type() >= QEvent::User && event->type() <= QEvent::MaxUser) {
    ExceptionEvent* exception_event(static_cast<ExceptionEvent*>(event));
    qDebug() << exception_event->ExceptionMessage();
    return true;
  }
  return QWidget::eventFilter(object, event);
}

void MainViewController::EventLoopStarted() {
  RefreshRequested(last_network_state_id_);
  show();
  view_.nodes_->setFocus();
  view_.open_data_viewer_->setFocus();
}

void MainViewController::RefreshRequested(int state_id) {
  // QtConcurrent::run(std::bind(&MainViewController::PopulateNodes, this, state_id));
  last_network_state_id_ = state_id;
  PopulateNodes();
}

void MainViewController::SelectionChanged() {
  if (view_.nodes_->selectedItems().isEmpty()) {
    main_page_->RenderGraph(last_network_state_id_, "", false);
  } else {
    auto temp(view_.nodes_->selectedItems().front());
    main_page_->RenderGraph(last_network_state_id_,
                            temp->data(Qt::UserRole).toString().toStdString(), false);
  }
}

void MainViewController::FilterChanged(const QString& new_filter) {
  foreach(QListWidgetItem* item, view_.nodes_->findItems(QString("*"), Qt::MatchWildcard))
    item->setHidden(!item->data(Qt::UserRole).toString().contains(new_filter));
}

void MainViewController::NewGraphViewRequested(const QString& new_parent_id) {
  CreateGraphController(new_parent_id, false);
}

void MainViewController::OpenDataViewer() {
  if (view_.data_id_->text().isEmpty()) {
    QMessageBox::critical(this, "Data Viewer", "Data ID cannot be empty");
    return;
  }
  CreateGraphController(view_.data_id_->text(), true);
}

void MainViewController::PopulateNodes() {
  std::vector<std::string> node_ids(api_helper_->GetNodesInNetwork(last_network_state_id_));
//  view_.nodes_->clear();
  std::vector<int> indices_for_removal;
  for (int i = 0; i < view_.nodes_->count(); ++i) {
    auto found_itr(std::find(std::begin(node_ids),
                   std::end(node_ids),
                   view_.nodes_->item(i)->data(Qt::UserRole).toString().toStdString()));
    if (found_itr == std::end(node_ids))
      indices_for_removal.push_back(i);
    else
      node_ids.erase(found_itr);
  }
  for (auto index : indices_for_removal)
    delete view_.nodes_->takeItem(index);

  for (std::string& node_id : node_ids) {
    QListWidgetItem* new_item(new QListWidgetItem(api_helper_->GetShortNodeId(node_id)));
    new_item->setData(Qt::UserRole, QString::fromStdString(node_id));
    view_.nodes_->addItem(new_item);
  }
  view_.count_->setText(QString::number(view_.nodes_->count()));
  SelectionChanged();
}

void MainViewController::CreateGraphController(const QString& new_parent_id, bool is_data_node) {
  GraphViewController* new_graph_controller(new GraphViewController(api_helper_));
  new_graph_controller->RenderNode(new_parent_id.toStdString(), is_data_node);
  new_graph_controller->show();
}

void MainViewController::InitSignals() {
  connect(api_helper_.get(),        SIGNAL(RequestGraphRefresh(int)),               // NOLINT - Viv
          this,                     SLOT(RefreshRequested(int)),                    // NOLINT - Viv
          Qt::QueuedConnection);
  connect(view_.nodes_,             SIGNAL(itemSelectionChanged()),
          this,                     SLOT(SelectionChanged()));
  connect(view_.filter_,            SIGNAL(textChanged(const QString&)),            // NOLINT - Viv
          this,                     SLOT(FilterChanged(const QString&)));           // NOLINT - Viv
  connect(main_page_,               SIGNAL(RequestNewGraphView(const QString&)),    // NOLINT - Viv
          this,                     SLOT(NewGraphViewRequested(const QString&)));   // NOLINT - Viv
  connect(view_.open_data_viewer_,  SIGNAL(clicked()),
          this,                     SLOT(OpenDataViewer()));
}

}  // namespace maidsafe
