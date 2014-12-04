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


#include <algorithm>
#include <memory>
#include <vector>

#include "boost/date_time/posix_time/posix_time.hpp"

#include "maidsafe/common/asio_service.h"
#include "maidsafe/common/log.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/api_config.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/acknowledgement.h"


namespace bptime = boost::posix_time;

namespace maidsafe {

namespace routing {

namespace test {

class AcknowledgementTest : public testing::Test {
 public:
  AcknowledgementTest()
      : local_node_id_(RandomString(NodeId::kSize)),
        asio_service_(2),
        acknowledgement_(local_node_id_, asio_service_),
        call_functor_(),
        message_() {
    call_functor_ = [=](const boost::system::error_code& error) {
                      if (error.value() == boost::system::errc::success) {
                        message_.set_id(message_.id() + 1);
                        acknowledgement_.Add(message_, call_functor_, Parameters::ack_timeout);
                      }
                    };

    message_.set_type(-200);
    message_.set_destination_id("destination_id");
    message_.set_direct(false);
    message_.add_data("response data");
    message_.set_source_id("source_id");
    message_.set_ack_id(acknowledgement_.GetId());
    message_.set_id(0);
  }

 protected:
  NodeId local_node_id_;
  AsioService asio_service_;
  Acknowledgement acknowledgement_;
  Handler call_functor_;
  protobuf::Message message_;
};

TEST_F(AcknowledgementTest, BEH_CallOnce) {
  acknowledgement_.Add(message_, call_functor_, Parameters::ack_timeout);
  Sleep(std::chrono::seconds(Parameters::ack_timeout + 1));
  acknowledgement_.Remove(message_.ack_id());
  EXPECT_EQ(1, message_.id());
}

TEST_F(AcknowledgementTest, BEH_CallTwice) {
  acknowledgement_.Add(message_, call_functor_, Parameters::ack_timeout);
  Sleep(std::chrono::seconds(Parameters::ack_timeout * 2 + 1));
  acknowledgement_.Remove(message_.ack_id());
  EXPECT_EQ(2, message_.id());
}

TEST_F(AcknowledgementTest, BEH_CallRemove) {
  acknowledgement_.Add(message_, call_functor_, Parameters::ack_timeout);
  Sleep(std::chrono::seconds(Parameters::ack_timeout * 2 + 1));
  EXPECT_EQ(2, message_.id());
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe


