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

#ifndef MAIDSAFE_ROUTING_MESSAGES_PUT_DATA_H_
#define MAIDSAFE_ROUTING_MESSAGES_PUT_DATA_H_

#include "maidsafe/common/config.h"
#include "maidsafe/common/data_types/data_type_values.h"
#include "maidsafe/common/serialisation/serialisation.h"

namespace maidsafe {

namespace routing {

class PutData {
 public:
  PutData() = default;
  ~PutData() = default;

  PutData(DataTagValue tag, SerialisedData data) : tag_(tag), data_(std::move(data)) {}

  PutData(PutData&& other) MAIDSAFE_NOEXCEPT : tag_(std::move(other.tag_)),
                                               data_(std::move(other.data_)) {}

  PutData& operator=(PutData&& other) MAIDSAFE_NOEXCEPT {
    tag_ = std::move(other.tag_);
    data_ = std::move(other.data_);
    return *this;
  }

  PutData(const PutData&) = delete;
  PutData& operator=(const PutData&) = delete;

  template <typename Archive>
  void serialize(Archive& archive) {
    archive(tag_, data_);
  }

  DataTagValue tag() const { return tag_; }
  SerialisedData data() const { return data_; }

 private:
  DataTagValue tag_;
  SerialisedData data_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGES_PUT_DATA_H_
