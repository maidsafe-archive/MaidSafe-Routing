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

#ifndef MAIDSAFE_ROUTING_FIREWALL_H_
#define MAIDSAFE_ROUTING_FIREWALL_H_

#include <time.h>
#include <tuple>
#include <deque>

#include "maidsafe/routing/api_config.h"

namespace maidsafe {

namespace routing {


namespace test {
  class FirewallTest_BEH_AddRemove_Test;
}

struct ProcessedMessageInfo {
  ProcessedMessageInfo(const NodeId& source_in, int32_t message_id)
      : source(source_in), id(message_id) {}
  NodeId source;
  int32_t id;
};

class Firewall {
 public:
  Firewall();
  Firewall& operator=(const Firewall&) = delete;
  Firewall& operator=(const Firewall&&) = delete;
  Firewall(const Firewall&) = delete;
  Firewall(const Firewall&&) = delete;

  bool Add(const NodeId& source_id, int32_t message_id);

 private:
  friend class test::FirewallTest_BEH_AddRemove_Test;

  std::mutex mutex_;
  std::deque<ProcessedMessageInfo> history_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_FIREWALL_H_

