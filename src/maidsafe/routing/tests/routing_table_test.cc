/* Copyright (c) 2009 maidsafe.net limited
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
    * Neither the name of the maidsafe.net limited nor the names of its
    contributors may be used to endorse or promote products derived from this
    software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <bitset>
#include <memory>
#include <vector>
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/transport/utils.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/maidsafe_routing_api.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/log.h"

namespace maidsafe {
namespace routing {
namespace test {

class RoutingTableTest {
  RoutingTableTest();
};
  
TEST(RoutingTableTest, BEH_AddCloseNodes) {
  protobuf::Contact contact;
  contact.set_node_id(RandomString(64));
  RoutingTable RT(contact);
   for (int i = 0; i < kClosestNodes ; ++i) {
     EXPECT_TRUE(RT.AddNode(NodeId(RandomString(64))));
   }
   EXPECT_EQ(RT.Size(), kClosestNodes);
}

TEST(RoutingTableTest, BEH_AddTooManyNodes) {
  protobuf::Contact contact;
  contact.set_node_id(RandomString(64));
  RoutingTable RT(contact);
   for (int i = 0; RT.Size() < kRoutingTableSize; ++i) {
     EXPECT_TRUE(RT.AddNode(NodeId(RandomString(64))));
   }
   EXPECT_EQ(RT.Size(), kRoutingTableSize);
   for (int i = 0; i < 100; ++i)
        if (RT.AddNode(NodeId(RandomString(64))))
          DLOG(INFO) << "made space for node in full routing table";
   EXPECT_EQ(RT.Size(), kRoutingTableSize);
}

TEST(RoutingTableTest, BEH_CloseAndInRangeCheck) {
  protobuf::Contact contact;
  contact.set_node_id(RandomString(64));
  RoutingTable RT(contact);
  NodeId my_node(NodeId(contact.node_id()));
  // Add some nodes to RT
  for (int i = 0; RT.Size() < kRoutingTableSize; ++i) {
    EXPECT_TRUE(RT.AddNode(NodeId(RandomString(64))));
  }
  EXPECT_EQ(RT.Size(), kRoutingTableSize);
  std::string my_id_encoded(my_node.ToStringEncoded(NodeId::kBinary));
  my_id_encoded[511] == '0' ? my_id_encoded[511] = '1' : my_id_encoded[511] = '0';
  NodeId my_closest_node(NodeId(my_id_encoded, NodeId::kBinary));
  EXPECT_TRUE(RT.AmIClosestNode(my_closest_node));
  EXPECT_TRUE(RT.IsMyNodeInRange(my_closest_node, 2));
  EXPECT_TRUE(RT.IsMyNodeInRange(my_closest_node, 200));  
  EXPECT_TRUE(RT.AmIClosestNode(my_closest_node));
  EXPECT_EQ(RT.Size(), kRoutingTableSize);
  // get closest nodes to me 
  std::vector<NodeId> close_nodes(RT.GetClosestNodes(my_node, kClosestNodes));
  // Check against individually selected close nodes
  for (int i = 0; i < kClosestNodes; ++i) 
    EXPECT_TRUE(std::find(close_nodes.begin(),
                          close_nodes.end(),
                          RT.GetClosestNode(my_node, i)) != close_nodes.end());
  // add the node now
  EXPECT_TRUE(RT.AddNode(my_closest_node));
  // houdl nwo be closest node to itself :-) 
  EXPECT_EQ(RT.GetClosestNode(my_closest_node).String(),
            my_closest_node.String());
  EXPECT_EQ(RT.Size(), kRoutingTableSize); // make sure we removed a
                                           // node to insert this one
}

// TODO really need transport or a fancy way around not having it :-(

// TEST(RoutingTableAPI, API_BadconfigFile) {
//   Routing RtAPI;
//   boost::filesystem3::path bad_file("bad file/ not found/ I hope");
//   EXPECT_FALSE(RtAPI.setConfigFilePath(bad_file));
// }

}  // namespace test
}  // namespace routing
}  // namespace maidsafe
