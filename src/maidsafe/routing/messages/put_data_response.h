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

#ifndef MAIDSAFE_ROUTING_MESSAGES_PUT_DATA_RESPONSE_H_
#define MAIDSAFE_ROUTING_MESSAGES_PUT_DATA_RESPONSE_H_

#include "maidsafe/common/config.h"
#include "maidsafe/common/error.h"
#include "maidsafe/common/data_types/data_type_values.h"
#include "maidsafe/common/serialisation/serialisation.h"

namespace maidsafe {

namespace routing {

class PutDataResponse {
 public:
  PutDataResponse() = default;
  ~PutDataResponse() = default;

  PutDataResponse(DataTagValue tag, SerialisedData data, maidsafe_error error)
      : tag_(tag), data_(std::move(data)), error_(std::move(error)) {}

  PutDataResponse(PutDataResponse&& other) MAIDSAFE_NOEXCEPT : tag_(std::move(other.tag_)),
                                                               data_(std::move(other.data_)),
                                                               error_(std::move(other.error_)) {}

  PutDataResponse& operator=(PutDataResponse&& other) MAIDSAFE_NOEXCEPT {
    tag_ = std::move(other.tag_);
    data_ = std::move(other.data_);
    error_ = std::move(other.error_);
    return *this;
  }

  PutDataResponse(const PutDataResponse&) = delete;
  PutDataResponse& operator=(const PutDataResponse&) = delete;

  template <typename Archive>
  void serialize(Archive& archive) {
    archive(tag_, data_, error_);
  }

  DataTagValue tag() const { return tag_; }
  SerialisedData data() const { return data_; }
  maidsafe_error error() const { return error_; }

 private:
  DataTagValue tag_;
  SerialisedData data_;
  maidsafe_error error_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGES_PUT_DATA_RESPONSE_H_
