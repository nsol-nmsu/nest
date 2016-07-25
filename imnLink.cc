#include "imnLink.h"

namespace ns3{

NS_OBJECT_ENSURE_REGISTERED (imnLink);

TypeId imnHelper::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::imnLink")
    .SetParent<Object> ()
    .SetGroupName ("topology-read")
    ;
  return tid;
}

}
  //this is a test
  //
