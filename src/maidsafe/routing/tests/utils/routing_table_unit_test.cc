/*  Copyright 2014 MaidSafe.net limited

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

#include "maidsafe/routing/tests/utils/routing_table_unit_test.h"

#include <bitset>
#include <string>

#include "maidsafe/passport/detail/fob.h"
#include "maidsafe/passport/types.h"
#include "maidsafe/common/utils.h"

namespace maidsafe {

namespace routing {

namespace test {

namespace {

enum class ContactType { kFar, kMid, kClose };

Address GetContact(const Address& furthest_from_tables_own_id, unsigned bucket_index,
                   ContactType contact_type) {
  std::bitset<Address::kSize * 8> binary_id{
      furthest_from_tables_own_id.ToStringEncoded(Address::EncodingType::kBinary)};
  if (bucket_index > 0) {
    for (unsigned i = (Address::kSize * 8) - bucket_index; i < Address::kSize * 8; ++i)
      binary_id.flip(i);
  }
  switch (contact_type) {
    case ContactType::kMid:
      binary_id.flip(0);
      break;
    case ContactType::kClose:
      binary_id.flip(1);
      break;
    case ContactType::kFar:  // no change to binary_id
    default:
      break;
  }
  return Address{binary_id.to_string(), Address::EncodingType::kBinary};
}

}  // unnamed namespace
RoutingTableUnitTest::Bucket::Bucket(const Address& furthest_from_tables_own_id, unsigned index_in)
    : index(index_in),
      far_contact(GetContact(furthest_from_tables_own_id, index, ContactType::kFar)),
      mid_contact(GetContact(furthest_from_tables_own_id, index, ContactType::kMid)),
      close_contact(GetContact(furthest_from_tables_own_id, index, ContactType::kClose)) {}

RoutingTableUnitTest::RoutingTableUnitTest()
    : our_id_(RandomString(Address::kSize)),
      fob_(passport::Pmid(passport::Anpmid())),
      public_fob_(passport::PublicPmid(fob_)),
      table_(our_id_),
      buckets_(InitialiseBuckets()),
      info_(our_id_, passport::PublicPmid{passport::Pmid(passport::Anpmid())}),
      initial_count_((RandomUint32() % (GroupSize - 1)) + 1),
      added_ids_() {
  for (int i = 0; i < 99; ++i) {
    EXPECT_TRUE(
        Address::CloserToTarget(buckets_[i].mid_contact, buckets_[i].far_contact, table_.OurId()))
        << "i == " << i;
    EXPECT_TRUE(
        Address::CloserToTarget(buckets_[i].close_contact, buckets_[i].mid_contact, table_.OurId()))
        << "i == " << i;
    EXPECT_TRUE(Address::CloserToTarget(buckets_[i + 1].far_contact, buckets_[i].close_contact,
                                        table_.OurId()))
        << "i == " << i;
  }
  EXPECT_TRUE(
      Address::CloserToTarget(buckets_[99].mid_contact, buckets_[99].far_contact, table_.OurId()));
  EXPECT_TRUE(Address::CloserToTarget(buckets_[99].close_contact, buckets_[99].mid_contact,
                                      table_.OurId()));

  const asymm::Keys keys(asymm::GenerateKeyPair());
  info_.dht_fob->public_key() = keys.public_key;
}

void RoutingTableUnitTest::PartiallyFillTable() {
  for (size_t i = 0; i < initial_count_; ++i) {
    info_.id = buckets_[i].mid_contact;
    added_ids_.push_back(info_.id);
    ASSERT_TRUE(table_.AddNode(info_).first);
  }
  ASSERT_EQ(initial_count_, table_.Size());
}

void RoutingTableUnitTest::CompleteFillingTable() {
  for (size_t i = initial_count_; i < RoutingTable::OptimalSize(); ++i) {
    info_.id = buckets_[i].mid_contact;
    added_ids_.push_back(info_.id);
    ASSERT_TRUE(table_.AddNode(info_).first);
  }
  ASSERT_EQ(RoutingTable::OptimalSize(), table_.Size());
}

RoutingTableUnitTest::Buckets RoutingTableUnitTest::InitialiseBuckets() {
  auto furthest_from_tables_own_id = table_.OurId() ^ Address { std::string(Address::kSize, -1) };
  Buckets the_buckets;
  for (unsigned i = 0; i < the_buckets.size(); ++i)
    the_buckets[i] = Bucket{furthest_from_tables_own_id, i};
  return the_buckets;
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
