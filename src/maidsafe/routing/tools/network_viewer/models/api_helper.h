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

#ifndef MAIDSAFE_ROUTING_TOOLS_NETWORK_VIEWER_MODELS_API_HELPER_H_
#define MAIDSAFE_ROUTING_TOOLS_NETWORK_VIEWER_MODELS_API_HELPER_H_

// std
#include <memory>
#include <string>
#include <functional>
#include <vector>

#include "maidsafe/routing/tools/network_viewer/helpers/qt_push_headers.h"
#include "maidsafe/routing/tools/network_viewer/helpers/qt_pop_headers.h"

namespace maidsafe {

namespace network_viewer { struct ViewableNode; }

class APIHelper : public QObject {
  Q_OBJECT

 public:
  explicit APIHelper(QObject* parent = nullptr);
  ~APIHelper();
  std::vector<std::string> GetNodesInNetwork(int state_id) const;
  std::vector<network_viewer::ViewableNode> GetCloseNodes(int state_id,
                                                          const std::string& id) const;
  void NetworkUpdated(int state_id);
  QString GetShortNodeId(std::string node_id) const;

 signals:
  void RequestGraphRefresh(int state_id);

 private:
  APIHelper(const APIHelper&);
  APIHelper& operator=(const APIHelper&);
  APIHelper(APIHelper&&);
  APIHelper& operator=(APIHelper&&);
};

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_TOOLS_NETWORK_VIEWER_MODELS_API_HELPER_H_
