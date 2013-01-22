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

#include "maidsafe/routing/processed_messages.h"

#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/parameters.h"

namespace maidsafe {

namespace routing {

namespace test {

TEST(ProcessedMessagesTest, BEH_AddRemove) {
  ProcessedMessages processed_messages;
  EXPECT_TRUE(processed_messages.Add((NodeId(NodeId::kRandomId)), RandomUint32()));
  EXPECT_EQ(processed_messages.history_.size(), 1);

  auto element(processed_messages.history_.at(0));

  EXPECT_FALSE(processed_messages.Add(std::get<0>(element), std::get<1>(element)));

  while (processed_messages.history_.size() < Parameters::message_history_cleanup_factor)
    processed_messages.Add((NodeId(NodeId::kRandomId)), RandomUint32());

  EXPECT_EQ(processed_messages.history_.size(), Parameters::message_history_cleanup_factor);

  Sleep(boost::posix_time::seconds(Parameters::message_age_to_drop + 1));

  EXPECT_TRUE(processed_messages.Add((NodeId(NodeId::kRandomId)), RandomUint32()));
  element = processed_messages.history_.back();

  EXPECT_EQ(processed_messages.history_.size(), 1);
  EXPECT_EQ(std::get<0>(element), std::get<0>(processed_messages.history_.at(0)));
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe

