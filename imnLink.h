#ifndef IMN_LINK_H
#define IMN_LINK_H

#include "ns3/object.h"
#include <string>
#include "imnNode.h"


using namespace std;

namespace ns3 {

class imnLink
{
  public:
    static TypeId GetTypeId (void); //recommended by ns-3 tutorial
    
    imnLink(){}
    virtual ~imnLink(){} //destructor
    
    string name;
    string type;
    string range;
    string bandwidth;
    string jitter;
    string delay;
    string error;
    string coordinates;
    vector<interface> interface_list;
    vector<string> peer_list;
    

};

}


#endif
