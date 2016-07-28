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
    
    imnLink();
    virtual ~imnLink(){} //destructor
    
    string name;
    string type;
    string range;
    string bandwidth;
    string jitter;
    string delay;
    string error; //ber for p2p in imn file
    string duplicate; //for p2p link only
    vector<interface> interface_list;
    vector<string> peer_list;
    position coordinates;
    vector<imnLink> extra_links;
    

};

}


#endif
