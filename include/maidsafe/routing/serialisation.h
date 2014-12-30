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

#ifndef MAIDSAFE_ROUTING_SERIALISATION_H_
#define MAIDSAFE_ROUTING_SERIALISATION_H_

#include <cstdint>
#include <limits>
#include <memory>
#include <string>

#include "cereal/archives/binary.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/vector.hpp"

#include "maidsafe/common/make_unique.h"
#include "maidsafe/common/types.h"
#include "maidsafe/common/serialisation/binary_archive.h"
#include "maidsafe/routing/compile_time_mapper.h"

namespace maidsafe {

namespace routing {

// ========== Serialisation/parsing helpers ========================================================
namespace detail {

template <typename StreamType>
struct Archive;

template <>
struct Archive<OutputVectorStream> {
  using type = BinaryOutputArchive;
};

template <>
struct Archive<InputVectorStream> {
  using type = BinaryInputArchive;
};

template <>
struct Archive<std::ostringstream> {
  using type = cereal::BinaryOutputArchive;
};

template <>
struct Archive<std::istringstream> {
  using type = cereal::BinaryInputArchive;
};

template <typename TypeToSerialise, typename StreamType>
std::unique_ptr<StreamType> Serialise(TypeToSerialise&& object_to_serialise) {
  auto binary_output_stream = maidsafe::make_unique<StreamType>();
  {
    typename Archive<StreamType>::type binary_output_archive(*binary_output_stream);
    binary_output_archive(GivenTypeFindTag_v<TypeToSerialise>::value,
                          std::forward<TypeToSerialise>(object_to_serialise));
  }
  return binary_output_stream;
}

template <typename StreamType>
MessageTypeTag TypeFromStream(StreamType& binary_input_stream) {
  MessageTypeTag tag;
  {
    typename Archive<StreamType>::type binary_input_archive(binary_input_stream);
    binary_input_archive(tag);
  }
  return tag;
}

template <typename ParsedType, typename StreamType>
ParsedType Parse(StreamType& binary_input_stream) {
  ParsedType parsed_message;
  {
    typename Archive<StreamType>::type binary_input_archive(binary_input_stream);
    binary_input_archive(parsed_message);
  }
  return parsed_message;
}

}  // namespace detail

// ========== Serialisation/parsing using std::vector<byte> types ==================================
template <typename TypeToSerialise>
SerialisedData SerialiseMappedType(TypeToSerialise object_to_serialise) {
  return detail::Serialise<TypeToSerialise, OutputVectorStream>(std::move(object_to_serialise))
      ->vector();
}

inline MessageTypeTag TypeFromStream(InputVectorStream& binary_input_stream) {
  return detail::TypeFromStream(binary_input_stream);
}



// ========== Serialisation/parsing using std::string types ========================================
template <typename TypeToSerialise>
std::string SerialiseMappedTypeToString(TypeToSerialise object_to_serialise) {
  return detail::Serialise<TypeToSerialise, std::ostringstream>(std::move(object_to_serialise))
      ->str();
}

inline MessageTypeTag TypeFromStringStream(std::istringstream& binary_input_stream) {
  return detail::TypeFromStream(binary_input_stream);
}

template <typename ParsedType>
ParsedType ParseFromStringStream(std::istringstream& binary_input_stream) {
  return detail::Parse<ParsedType, std::istringstream>(binary_input_stream);
}

}  // routing

}  // maidsafe

#endif  // MAIDSAFE_ROUTING_SERIALISATION_H_
