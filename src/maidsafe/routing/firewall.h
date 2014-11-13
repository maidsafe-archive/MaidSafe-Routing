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

#include "boost/multi_index_container.hpp"
#include "boost/multi_index/global_fun.hpp"
#include "boost/multi_index/member.hpp"
#include "boost/multi_index/ordered_index.hpp"
#include "boost/multi_index/identity.hpp"

#include "maidsafe/common/clock.h"
#include "maidsafe/routing/api_config.h"

namespace maidsafe {

namespace routing {


namespace test {
  class FirewallTest_BEH_AddRemove_Test;
}

class Firewall {
 public:
  Firewall();
  Firewall& operator=(const Firewall&) = delete;
  Firewall& operator=(const Firewall&&) = delete;
  Firewall(const Firewall&) = delete;
  Firewall(const Firewall&&) = delete;

  bool Add(const NodeId& source_id, int32_t message_id);
  void Remove(std::unique_lock<std::mutex>& lock);

  struct ProcessedEntry {
    ProcessedEntry(const NodeId& source_in, int32_t messsage_id_in)
        : source(source_in), message_id(messsage_id_in), birth_time(common::Clock::now()) {}
    ProcessedEntry Key() const { return *this; }
    NodeId source;
    int32_t message_id;
    common::Clock::time_point birth_time;
  };

 private:
  friend class test::FirewallTest_BEH_AddRemove_Test;

  struct BirthTimeTag {};

  typedef boost::multi_index_container<
      ProcessedEntry,
      boost::multi_index::indexed_by<
          boost::multi_index::ordered_unique<boost::multi_index::identity<ProcessedEntry>>,
      boost::multi_index::ordered_non_unique<
          boost::multi_index::tag<BirthTimeTag>,
          BOOST_MULTI_INDEX_MEMBER(ProcessedEntry, common::Clock::time_point, birth_time)>
    >
  > ProcessedEntrySet;

  std::mutex mutex_;
  ProcessedEntrySet history_;
};

bool operator > (const Firewall::ProcessedEntry& lhs, const Firewall::ProcessedEntry& rhs);

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_FIREWALL_H_
