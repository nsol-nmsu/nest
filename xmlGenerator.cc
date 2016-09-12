

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
  imnHelper imn_container(file_name.c_str()); //holds entire imn file in a list of nodes and list of links containers
  
  //pt::ptree tree;  //use boost property_tree
  

  //some values are placeholders for now, for testing purposes
  pt::ptree tree;
  tree.add("EmulationScript.<xmlattr>.version", "0.1");
  tree.add("EmulationScript.Event", "");
  tree.add("EmulationScript.Event.time", "0.0");
  
  //Generate NetworkPlan
  tree.add("EmulationScript.Event.NetworkPlan", "");
  
  for(int i = 0; i < imn_container.imn_nodes.size(); i++){
    pt::ptree& Node = tree.add("EmulationScript.Event.NetworkPlan.Node", "");
    Node.add("<xmlattr>.name", imn_container.imn_nodes.at(i).name);
    //Node.add("CORE_type", imn_container.imn_nodes.at(i).type);
    //Node.add("CORE_model", imn_container.imn_nodes.at(i).model); 
    //type and model should be in defined during servicePlan?
    //think they will be used to infer services running only, not explicitly stated
    
    //Generate non-link-type Nodes (i.e routers,...ect)
    for(int j = 0; j < imn_container.imn_nodes.at(i).interface_list.size(); j++){
      pt::ptree& interface = Node.add("interface", "");
      interface.add("<xmlattr>.name", imn_container.imn_nodes.at(i).interface_list.at(j).interface_name);
      interface.add("<xmlattr>.type", imn_container.imn_nodes.at(i).type); //using Node type, check if this is correct
      
      for(int addr_num = 0; addr_num < 3; addr_num++){
        if(addr_num == 0 && imn_container.imn_nodes.at(i).interface_list.at(j).ipv4_addr.compare("") != 0){ //check if empty
          pt::ptree& address = interface.add("address", imn_container.imn_nodes.at(i).interface_list.at(j).ipv4_addr);
          address.add("<xmlattr>.type", "ipv4");
        }
        else  if(addr_num == 1 && imn_container.imn_nodes.at(i).interface_list.at(j).ipv6_addr.compare("") != 0){ //check if empty
          pt::ptree& address = interface.add("address", imn_container.imn_nodes.at(i).interface_list.at(j).ipv6_addr);
          address.add("<xmlattr>.type", "ipv6");
        }
        else  if(addr_num == 2 && imn_container.imn_nodes.at(i).interface_list.at(j).mac_addr.compare("") != 0){ //check if empty
          pt::ptree& address = interface.add("address", imn_container.imn_nodes.at(i).interface_list.at(j).mac_addr);
          address.add("<xmlattr>.type", "mac");
        }
      }
      interface.add("peer.<xmlattr>.name", imn_container.imn_nodes.at(i).interface_list.at(j).peer);
    }
  }
  //Generate Link-type Nodes (i.e wifi,hubs,lanswitch,..ect)
  for(int i = 0; i < imn_container.imn_links.size(); i++){
    pt::ptree& Node = tree.add("EmulationScript.Event.NetworkPlan.Node", "");
    Node.add("<xmlattr>.name", imn_container.imn_links.at(i).name);
    
    //handle wlan
    if(imn_container.imn_links.at(i).type.compare("wlan") == 0){
      for(int j = 0; j < imn_container.imn_links.at(i).interface_list.size(); j++){
        pt::ptree& interface = Node.add("interface", "");
        interface.add("<xmlattr>.name", imn_container.imn_links.at(i).interface_list.at(j).interface_name);
        interface.add("<xmlattr>.type", imn_container.imn_links.at(i).type);
        
        for(int addr_num = 0; addr_num < 3; addr_num++){
          if(addr_num == 0 && imn_container.imn_links.at(i).interface_list.at(j).ipv4_addr.compare("") != 0){ //check if empty
           pt::ptree& address = interface.add("address", imn_container.imn_links.at(i).interface_list.at(j).ipv4_addr);
           address.add("<xmlattr>.type", "ipv4");
          }
          else  if(addr_num == 1 && imn_container.imn_links.at(i).interface_list.at(j).ipv6_addr.compare("") != 0){ //check if empty
            pt::ptree& address = interface.add("address", imn_container.imn_links.at(i).interface_list.at(j).ipv6_addr);
            address.add("<xmlattr>.type", "ipv6");
          }
          else  if(addr_num == 2 && imn_container.imn_links.at(i).interface_list.at(j).mac_addr.compare("") != 0){ //check if empty
           pt::ptree& address = interface.add("address", imn_container.imn_links.at(i).interface_list.at(j).mac_addr);
           address.add("<xmlattr>.type", "mac");
          }
        }
        
        pt::ptree& channel = interface.add("channel", "");
        channel.add("bandwidth", imn_container.imn_links.at(i).bandwidth);
        channel.add("range", imn_container.imn_links.at(i).range);
        channel.add("jitter", imn_container.imn_links.at(i).jitter);
        channel.add("delay", imn_container.imn_links.at(i).delay);
        channel.add("error", imn_container.imn_links.at(i).error);
        channel.add("duplicate", imn_container.imn_links.at(i).duplicate);
        for(int j = 0; j < imn_container.imn_links.at(i).peer_list.size(); j++){
          pt::ptree& peer = channel.add("peer","");
          peer.add("<xmlattr>.name", imn_container.imn_links.at(i).peer_list.at(j));
        }
      }
    }
    else if(imn_container.imn_links.at(i).type.compare("p2p") == 0){ //process p2p links
      pt::ptree& interface = Node.add("interface", "");
      interface.add("<xmlattr>.type", imn_container.imn_links.at(i).type);
      pt::ptree& channel = interface.add("channel", "");
      channel.add("bandwidth", imn_container.imn_links.at(i).bandwidth);
      channel.add("range", imn_container.imn_links.at(i).range);
      channel.add("jitter", imn_container.imn_links.at(i).jitter);
      channel.add("delay", imn_container.imn_links.at(i).delay);
      channel.add("error", imn_container.imn_links.at(i).error);
      channel.add("duplicate", imn_container.imn_links.at(i).duplicate);
      for(int j = 0; j < imn_container.imn_links.at(i).peer_list.size(); j++){
        pt::ptree& peer = channel.add("peer","");
        peer.add("<xmlattr>.name", imn_container.imn_links.at(i).peer_list.at(j));
      }
    }
    else{
      pt::ptree& interface = Node.add("interface", "");
      interface.add("<xmlattr>.type", imn_container.imn_links.at(i).type);
      
      for(int j = 0; j < imn_container.imn_links.at(i).extra_links.size(); j++){
        pt::ptree& channel = interface.add("channel", "");
        channel.add("bandwidth", imn_container.imn_links.at(i).extra_links.at(j).bandwidth);
        channel.add("range", imn_container.imn_links.at(i).extra_links.at(j).range);
        channel.add("jitter", imn_container.imn_links.at(i).extra_links.at(j).jitter);
        channel.add("delay", imn_container.imn_links.at(i).extra_links.at(j).delay);
        channel.add("error", imn_container.imn_links.at(i).extra_links.at(j).error);
        channel.add("duplicate", imn_container.imn_links.at(i).extra_links.at(j).duplicate);
        for(int x = 0; x < imn_container.imn_links.at(i).extra_links.at(j).peer_list.size(); x++){
          pt::ptree& peer = channel.add("peer","");
          peer.add("<xmlattr>.name", imn_container.imn_links.at(i).extra_links.at(j).peer_list.at(x));
        }
      }
    }
    
  }
  
  //Generate MotionPlan
  tree.add("EmulationScript.Event.MotionPlan", "");

  write_xml("my_test.xml", tree, std::locale(),pt::xml_writer_settings<string>(' ', 4));

  
  

}




} //end namespace ns3
