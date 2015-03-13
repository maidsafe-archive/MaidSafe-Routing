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

#ifndef MAIDSAFE_ROUTING_MESSAGES_GET_DATA_RESPONSE_H_
#define MAIDSAFE_ROUTING_MESSAGES_GET_DATA_RESPONSE_H_

#include "boost/optional/optional.hpp"

#include "maidsafe/common/config.h"
#include "maidsafe/common/error.h"
#include "maidsafe/common/types.h"
#include "maidsafe/common/data_types/data.h"
#include "maidsafe/common/serialisation/serialisation.h"

namespace maidsafe {

namespace routing {

class GetDataResponse {
 public:
  GetDataResponse() = default;
  ~GetDataResponse() = default;

  GetDataResponse(Data::NameAndTypeId name_and_type_id, SerialisedData&& data)
      : name_and_type_id_(std::move(name_and_type_id)), data_(std::move(data)), error_() {}

  GetDataResponse(Data::NameAndTypeId name_and_type_id, maidsafe_error error)
      : name_and_type_id_(std::move(name_and_type_id)), data_(), error_(error) {}

  GetDataResponse(GetDataResponse&& other) MAIDSAFE_NOEXCEPT
      : name_and_type_id_(std::move(other.name_and_type_id_)),
        data_(std::move(other.data_)),
        error_(std::move(other.error_)) {}

  GetDataResponse& operator=(GetDataResponse&& other) MAIDSAFE_NOEXCEPT {
    name_and_type_id_ = std::move(other.name_and_type_id_);
    data_ = std::move(other.data_);
    error_ = std::move(other.error_);
    return *this;
  }

  GetDataResponse(const GetDataResponse&) = delete;
  GetDataResponse& operator=(const GetDataResponse&) = delete;

  template <typename Archive>
  void serialize(Archive& archive) {
    archive(name_and_type_id_, data_, error_);
  }

  Data::NameAndTypeId name_and_type_id() const { return name_and_type_id_; }
  boost::optional<SerialisedData> data() const { return data_; }
  boost::optional<maidsafe_error> error() const { return error_; }

 private:
  Data::NameAndTypeId name_and_type_id_;
  boost::optional<SerialisedData> data_;
  boost::optional<maidsafe_error> error_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGES_GET_DATA_RESPONSE_H_
