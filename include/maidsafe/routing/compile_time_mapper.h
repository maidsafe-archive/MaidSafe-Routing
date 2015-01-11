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

#ifndef MAIDSAFE_ROUTING_COMPILE_TIME_MAPPER_H_
#define MAIDSAFE_ROUTING_COMPILE_TIME_MAPPER_H_

#include <type_traits>

#include "maidsafe/routing/messages/messages_fwd.h"

namespace maidsafe {

namespace routing {

struct CompileTimeMapper {
 private:
  template <typename T>
  using remove_ref_cv_t = typename std::remove_cv<typename std::remove_reference<T>::type>::type;

  // ****************************************************************
  //                        Map Segment
  // ****************************************************************
  template <typename Data, typename NextNode>
  struct Node;
  struct ERROR_Entry_Not_Found_In_Map;

  template <MessageTypeTag, typename>
  struct Pair;

  template <typename...>
  struct CompileTimeMap;

  template <MessageTypeTag tag, typename Type, typename... Pairs>
  struct CompileTimeMap<Pair<tag, Type>, Pairs...> {
    using Map = Node<Pair<tag, Type>, typename CompileTimeMap<Pairs...>::Map>;
  };

  template <MessageTypeTag tag, typename Type>
  struct CompileTimeMap<Pair<tag, Type>> {
    using Map = Node<Pair<tag, Type>, ERROR_Entry_Not_Found_In_Map>;
  };

  // ****************************************************************
  //                        Search Segment
  // ****************************************************************
  template <MessageTypeTag, typename>
  struct GivenTagFindType;

  template <MessageTypeTag GivenTag, MessageTypeTag tag, typename Type, typename NextNode>
  struct GivenTagFindType<GivenTag, Node<Pair<tag, Type>, NextNode>>
      : GivenTagFindType<GivenTag, NextNode> {};

  template <MessageTypeTag GivenTag, typename Type, typename NextNode>
  struct GivenTagFindType<GivenTag, Node<Pair<GivenTag, Type>, NextNode>> {
    using type = Type;
  };

  template <MessageTypeTag GivenTag>
  struct GivenTagFindType<GivenTag, ERROR_Entry_Not_Found_In_Map> {};

  // ----------------------------------------------------------------

  template <typename, typename>
  struct GivenTypeFindTag;

  template <typename GivenType, MessageTypeTag tag, typename Type, typename NextNode>
  struct GivenTypeFindTag<GivenType, Node<Pair<tag, Type>, NextNode>>
      : GivenTypeFindTag<GivenType, NextNode> {};

  template <typename GivenType, MessageTypeTag tag, typename NextNode>
  struct GivenTypeFindTag<GivenType, Node<Pair<tag, GivenType>, NextNode>> {
    static const MessageTypeTag value{tag};
  };

  template <typename GivenType>
  struct GivenTypeFindTag<GivenType, ERROR_Entry_Not_Found_In_Map> {};

  // ****************************************************************
  //                        Populate Segment
  // ****************************************************************
  using RootNode = CompileTimeMap<
      Pair<MessageTypeTag::Connect, Connect>,
      Pair<MessageTypeTag::ConnectResponse, ConnectResponse>,
      Pair<MessageTypeTag::ClientConnect, ClientConnect>,
      Pair<MessageTypeTag::ClientConnectResponse, ClientConnectResponse>,
      Pair<MessageTypeTag::FindGroup, FindGroup>,
      Pair<MessageTypeTag::FindGroupResponse, FindGroupResponse>,
      Pair<MessageTypeTag::GetData, GetData>,
      Pair<MessageTypeTag::GetDataResponse, GetDataResponse>,
      Pair<MessageTypeTag::PutData, PutData>,
      Pair<MessageTypeTag::PutDataResponse, PutDataResponse>,
      Pair<MessageTypeTag::ClientPutData, ClientPutData>, Pair<MessageTypeTag::PutKey, PutKey>,
      Pair<MessageTypeTag::Post, Post>, Pair<MessageTypeTag::ClientPost, ClientPost>,
      Pair<MessageTypeTag::ClientRequest, ClientRequest>,
      Pair<MessageTypeTag::ClientResponse, ClientResponse>, Pair<MessageTypeTag::Request, Request>,
      Pair<MessageTypeTag::Response, Response>>::Map;

 public:
  CompileTimeMapper() = delete;

  // ****************************************************************
  //                        Convenience Segment
  // ****************************************************************
  template <MessageTypeTag tag>
  using GivenTagFindType_t = typename GivenTagFindType<tag, RootNode>::type;

  template <typename Type>
  struct GivenTypeFindTag_v : GivenTypeFindTag<remove_ref_cv_t<Type>, RootNode> {};
};


// Following are not necessary - Just for convenience to avoid typing CompileTimeMapper.
template <MessageTypeTag tag>
using GivenTagFindType_t = CompileTimeMapper::GivenTagFindType_t<tag>;

template <typename Type>
using GivenTypeFindTag_v = CompileTimeMapper::GivenTypeFindTag_v<Type>;

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_COMPILE_TIME_MAPPER_H_
