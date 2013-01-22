/*******************************************************************************
 *  Copyright 2012 maidsafe.net limited                                        *
 *                                                                             *
 *  The following source code is property of maidsafe.net limited and is not   *
 *  meant for external use.  The use of this code is governed by the licence   *
 *  file licence.txt found in the root of this directory and also on           *
 *  www.maidsafe.net.                                                          *
 *                                                                             *
 *  You are not free to copy, amend or otherwise use this source code without  *
 *  the explicit written permission of the board of directors of maidsafe.net. *
 ******************************************************************************/

#ifndef MAIDSAFE_ROUTING_PROCESSED_MESSAGES_H_
#define MAIDSAFE_ROUTING_PROCESSED_MESSAGES_H_

#include <time.h>
#include <tuple>
#include <vector>

#include "maidsafe/routing/api_config.h"

namespace maidsafe {

namespace routing {


namespace test {
  class ProcessedMessagesTest_BEH_AddRemove_Test;
}

typedef std::tuple<NodeId, uint32_t, std::time_t> ProcessedMessage;

class ProcessedMessages {
 public:
  ProcessedMessages();
  bool Add(const NodeId& source_id, const uint32_t& message_id);

 private:
  friend class test::ProcessedMessagesTest_BEH_AddRemove_Test;

  ProcessedMessages &operator=(const ProcessedMessages&);
  ProcessedMessages(const ProcessedMessages&);
  ProcessedMessages(const ProcessedMessages&&);
  void Remove(std::unique_lock<std::mutex>& lock);

  std::mutex mutex_;
  std::vector<ProcessedMessage> history_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_PROCESSED_MESSAGES_H_
