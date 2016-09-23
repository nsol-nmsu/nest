

#include "xmlGenerator.h"


namespace ns3{

NS_OBJECT_ENSURE_REGISTERED (xmlGenerator);

TypeId xmlGenerator::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::xmlGenerator")
    .SetParent<Object> ()
    .SetGroupName ("topology-read")
    ;
  return tid;
}

//constructor
//parse_mode needed to differentiate between format types that can be parsed
xmlGenerator::xmlGenerator(int parse_mode, string fname){
  
  file_name = fname;
  generate_xml(parse_mode);

}

//destructor
xmlGenerator::~xmlGenerator ()
{

}

void xmlGenerator::generate_xml(int parse_mode){
  
  //parse imn, generate xml 
  if(parse_mode == TYPE_IMN){
    generate_from_imn();
  }else{ //TODO: Add more parsing options. Different file types with scenarios that we need xml's generated for.
    
  }
  
}


void xmlGenerator::generate_from_imn(){

  //call imnHelper to parse imn File, then use the sturctures created to populate xml
  imnHelper imn_container(file_name.c_str());

  //some values are placeholders for now, for testing purposes
  pt::ptree tree;
  tree.add("ScenarioScript.<xmlattr>.version", "0.1");
  tree.add("ScenarioScript.<xmlattr>.name", "TEST");
  tree.add("ScenarioScript.<xmlattr>.platform", "CORE");
  tree.add("ScenarioScript.Event", "");
  tree.add("ScenarioScript.Event.time", "0.0");
  
  //Generate NetworkPlan
  generateNetworkPlan(tree, imn_container);

  write_xml("my_test.xml", tree, std::locale(),pt::xml_writer_settings<string>(' ', 4));

  
}



//TODO: clean up method a bit
void xmlGenerator::generateNetworkPlan(pt::ptree& current_tree, imnHelper& imn_c){
  
  current_tree.add("ScenarioScript.NetworkPlan", "");
  
  //Generate non-link-type Nodes (i.e routers,...ect) 
  for(int i = 0; i < imn_c.imn_nodes.size(); i++){
    pt::ptree& Node = current_tree.add("ScenarioScript.NetworkPlan.Node", "");
    Node.add("<xmlattr>.name", imn_c.imn_nodes.at(i).name);
    
    //Add all location information
    addNetPlanLocation(Node, imn_c.imn_nodes.at(i));
    
    //Add all interface information
    addNetPlanInterface(Node, imn_c.imn_nodes.at(i));
  }
  
  //Generate Link-type Nodes (i.e wifi,hubs,lanswitch,..ect)
  for(int i = 0; i < imn_c.imn_links.size(); i++){
    pt::ptree& Node = current_tree.add("ScenarioScript.NetworkPlan.Node", "");
    Node.add("<xmlattr>.name", imn_c.imn_links.at(i).name);
    
    //Add all location information
    if(imn_c.imn_links.at(i).type.compare("p2p") != 0){
      addNetPlanLocation(Node, imn_c.imn_links.at(i));  
    }
    
    //Add all interface information
    addNetPlanInterface(Node, imn_c.imn_links.at(i));
  }
  
}

//function to add location information for a link
void xmlGenerator:: addNetPlanLocation(pt::ptree& current_tree, imnLink l)
{

    ostringstream ss, ss2;
    ss << l.coordinates.x;
    string first(ss.str());
    ss2 << l.coordinates.y;
    string second(ss2.str());
    string loc = first + "," + second;
    
    pt::ptree& location = current_tree.add("Location", loc);
    location.add("<xmlattr>.type", l.coordinates.type);
    //location.add("<xmlattr>.duration", "");
}

//function to add location inforamtion for a node
void xmlGenerator:: addNetPlanLocation(pt::ptree& current_tree, imnNode n)
{
    
    ostringstream ss, ss2;
    ss << n.coordinates.x;
    string first(ss.str());
    ss2 << n.coordinates.y;
    string second(ss2.str());
    string loc = first + "," + second;
    
    pt::ptree& location = current_tree.add("Location", loc);
    location.add("<xmlattr>.type", n.coordinates.type);
    //location.add("<xmlattr>.duration", "");

}

//function to add Interface information for node interface
void xmlGenerator:: addNetPlanInterface(pt::ptree& current_tree, imnNode n)
{
  for(int i = 0; i < n.interface_list.size(); i++){
    pt::ptree& interface = current_tree.add("interface", "");
    interface.add("<xmlattr>.name", n.interface_list.at(i).interface_name);
    //interface.add("<xmlattr>.type", n.type); //TODO:using Node type, schema asks for different input, not retrivable at this time

    
    for(int addr_num = 0; addr_num < 3; addr_num++){ //add ipv4, ipv6, and mac addr
      if(addr_num == 0 && n.interface_list.at(i).ipv4_addr.compare("") != 0){ //check if empty
        pt::ptree& address = interface.add("address", n.interface_list.at(i).ipv4_addr);
        address.add("<xmlattr>.type", "ipv4");
      }
      else  if(addr_num == 1 && n.interface_list.at(i).ipv6_addr.compare("") != 0){ //check if empty
        pt::ptree& address = interface.add("address", n.interface_list.at(i).ipv6_addr);
        address.add("<xmlattr>.type", "ipv6");
      }
      else  if(addr_num == 2 && n.interface_list.at(i).mac_addr.compare("") != 0){ //check if empty
        pt::ptree& address = interface.add("address", n.interface_list.at(i).mac_addr);
        address.add("<xmlattr>.type", "mac");
      }
    }
    interface.add("peer.<xmlattr>.name", n.interface_list.at(i).peer);
  }
}

//funciton to add Interface information for link interface
void xmlGenerator:: addNetPlanInterface(pt::ptree& current_tree, imnLink l)
{
  
  //handle wlan's list of interfaces
  if(l.type.compare("wlan") == 0){
    for(int i = 0; i < l.interface_list.size(); i++){
      pt::ptree& interface = current_tree.add("interface", "");
      interface.add("<xmlattr>.name", l.interface_list.at(i).interface_name);
      interface.add("<xmlattr>.type", l.type);
        
      for(int addr_num = 0; addr_num < 3; addr_num++){
        if(addr_num == 0 && l.interface_list.at(i).ipv4_addr.compare("") != 0){ //check if empty
         pt::ptree& address = interface.add("address", l.interface_list.at(i).ipv4_addr);
         address.add("<xmlattr>.type", "ipv4");
        }
        else  if(addr_num == 1 && l.interface_list.at(i).ipv6_addr.compare("") != 0){ //check if empty
          pt::ptree& address = interface.add("address", l.interface_list.at(i).ipv6_addr);
          address.add("<xmlattr>.type", "ipv6");
        }
        else  if(addr_num == 2 && l.interface_list.at(i).mac_addr.compare("") != 0){ //check if empty
         pt::ptree& address = interface.add("address", l.interface_list.at(i).mac_addr);
         address.add("<xmlattr>.type", "mac");
        }
      }
      pt::ptree& channel = interface.add("channel", "");
      channel.add("bandwidth", l.bandwidth);
      channel.add("range", l.range);
      channel.add("jitter", l.jitter);
      channel.add("delay", l.delay);
      channel.add("error", l.error);
      channel.add("duplicate", l.duplicate);
      for(int i = 0; i < l.peer_list.size(); i++){
        pt::ptree& peer = channel.add("peer","");
        peer.add("<xmlattr>.name", l.peer_list.at(i));
      }
    }
  }
  else{
    pt::ptree& interface = current_tree.add("interface", "");
    interface.add("<xmlattr>.type", l.type);
    
    //add p2p channel
    if(l.type.compare("p2p") == 0){
      pt::ptree& channel = interface.add("channel", "");
      channel.add("bandwidth", l.bandwidth);
      channel.add("range", l.range);
      channel.add("jitter", l.jitter);
      channel.add("delay", l.delay);
      channel.add("error", l.error);
      channel.add("duplicate", l.duplicate);
      for(int i = 0; i < l.peer_list.size(); i++){
        pt::ptree& peer = channel.add("peer","");
        peer.add("<xmlattr>.name", l.peer_list.at(i));
      }
    }
    else{ //add all channels that a hub/switch has
      for(int i = 0; i < l.extra_links.size(); i++){
        pt::ptree& channel = interface.add("channel", "");
        channel.add("bandwidth", l.extra_links.at(i).bandwidth);
        channel.add("range", l.extra_links.at(i).range);
        channel.add("jitter", l.extra_links.at(i).jitter);
        channel.add("delay", l.extra_links.at(i).delay);
        channel.add("error", l.extra_links.at(i).error);
        channel.add("duplicate", l.extra_links.at(i).duplicate);
        for(int x = 0; x < l.extra_links.at(i).peer_list.size(); x++){
          pt::ptree& peer = channel.add("peer","");
          peer.add("<xmlattr>.name", l.extra_links.at(i).peer_list.at(x));
        }
      }
    }
  }
}


//method to generate the servicePlan
void xmlGenerator:: generateServicePlan(pt::ptree& current_tree, imnHelper& imn_c)
{

}




} //end namespace ns3

