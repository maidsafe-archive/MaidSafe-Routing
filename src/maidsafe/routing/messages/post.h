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

#ifndef MAIDSAFE_ROUTING_MESSAGES_POST_H_
#define MAIDSAFE_ROUTING_MESSAGES_POST_H_

#include "maidsafe/common/config.h"
#include "maidsafe/common/types.h"
#include "maidsafe/common/data_types/data.h"
#include "maidsafe/common/serialisation/serialisation.h"

namespace maidsafe {

namespace routing {

class Post {
 public:
  Post() = default;
  ~Post() = default;

  Post(Data::NameAndTypeId name_and_type_id, SerialisedData data)
      : name_and_type_id_(std::move(name_and_type_id)), data_(std::move(data)) {}

  Post(Post&& other) MAIDSAFE_NOEXCEPT : name_and_type_id_(std::move(other.name_and_type_id_)),
                                         data_(std::move(other.data_)) {}

  Post& operator=(Post&& other) MAIDSAFE_NOEXCEPT {
    name_and_type_id_ = std::move(other.name_and_type_id_);
    data_ = std::move(other.data_);
    return *this;
  }

  Post(const Post&) = delete;
  Post& operator=(const Post&) = delete;

  template <typename Archive>
  void serialize(Archive& archive) {
    archive(name_and_type_id_, data_);
  }

  Data::NameAndTypeId name_and_type_id() const { return name_and_type_id_; }
  SerialisedData data() const { return data_; }

 private:
  Data::NameAndTypeId name_and_type_id_;
  SerialisedData data_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGES_POST_H_
