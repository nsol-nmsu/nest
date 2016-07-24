#include "imnNode.h"

namespace ns3{

NS_OBJECT_ENSURE_REGISTERED (imnNode);

TypeId imnHelper::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::imnNode")
    .SetParent<Object> ()
    .SetGroupName ("topology-read")
    ;
  return tid;
}


}

