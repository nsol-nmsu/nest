#ifndef IMN_NODE_H
#define IMN_NODE_H

#include "ns3/object.h"
#include <string>

/* class representation of a node defined in .imn file, will act basically as a container for information
regarding that specific node
*/

using namespace std;

namespace ns3 {

class interface
{
  public:
    string interface_name;
    string ipv4_addr;
    string ipv6_addr;
    string mac_addr;
};

class imnNode
{
  public:
    static TypeId GetTypeId (void); //recommended by ns-3 tutorial
    
    imnNode();
    virtual ~imnNode(){} //destructor
    
    string name;
    string type;
    string model;
    string coordinates;
    vector<interface> interface_list;
    

};


}


#endif
