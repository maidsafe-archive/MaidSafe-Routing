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
