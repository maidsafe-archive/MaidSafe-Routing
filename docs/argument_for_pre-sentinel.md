## Argument for NOT accumulating on GetKey and GetGroupKey

If Sentinel `::Add()`s `GetKey` and `GetGroupKey` to the accumulator then that producers an infinite loop that is cut out by the `filter`.  At that point the logic blocks and the message would never reach beyond the first `Sentinel`

    < Client {
        Generate payload
        Sign payload  // currently not the case
        Generate message (payload, signature)
        Sign message
        Assign message id
        Generate header (message id, signature, source=client node)
        Send to Client Manager
      }
    | Client Manager {
        Filter on message id + source=client node
        Swarm
        Sentinel 1 {
          Receive single message
          GetKey
           - to this Client Manager group
           - from each manager, to all other managers
           - preserve message id
           - add signature
          {
            Filter on same message id + source=group node from client manager
                             --> add MessageTypeTag to FilterValue ?
                             --> otherwise GetKey is blocked
            Swarm
            Sentinel 2 {
              Receive single message GetKey
              GetKey
               - to this Client Manager group
               - preserve message id
               - add signature
              {
                Filter on same message id + source=group node from client manager
                ##! FILTER BLOCKS !##
                --> GetKey and GetGroupKey have to be handled pre-sentinel,
                    ie, Sentinel DOES NOT ACUMMULATE on these message types ?
                --> is there value in entering Sentinel level 2 ?
              }
              Receive GetKey from other nodes Sentinel level 1
              Accumulate only GetKey messages, no GetKeyResponse messages
              ##! LOGIC BLOCKS !##
              --> GetKey, GetGroupKey, PutKey are pre-sentinel messages
            }
          }
        }
      }
    | NEVER REACHES >
