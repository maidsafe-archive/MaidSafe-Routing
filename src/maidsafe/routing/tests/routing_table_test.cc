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

#include "maidsafe/common/test.h"

#include "maidsafe/common/utils.h"
#include "maidsafe/transport/utils.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/maidsafe_routing.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/node_id.h"

namespace maidsafe {

namespace routing {

namespace test {

class RoutingTableTest {
  RoutingTableTest();
};
  
TEST(RoutingTableTest, BEH_AddLessThanClose) {
  RoutingTable RT(NodeId(kRandomId));
  std::vector<Contact> contacts(kClosestNodes);
//   for (int i = 0; i < kClosestNodes - 1; ++i) {
//     NodeId node`(NodeId(kRandomId));
//     contacts.at(i).set_node_id(node.ToStringEncoded());
//     RT.AddContact(contacts.at(i));
//   }
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
