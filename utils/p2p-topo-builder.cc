#include "topo-builder.h"

void p2pBuilder(ptree pt, const ptree& child, NodeContainer nodes, NetDeviceContainer nd, bool pcap){
      optional<const ptree&> channel_exists = child.get_child_optional("channel");

      // CORE occasionally produces empty p2p networks, we skip them
      if(!channel_exists){
        return;
      }

      //NS_LOG_INFO ("Create Point to Point channel.");
      NodeContainer p2pNodes;
      NetDeviceContainer p2pDevices;
      PointToPointHelper p2p;

      string name_holder, name_holder2, pType, p2Type, peer, peer2;
      bool fst = true;

      // grab the node names from net
      BOOST_FOREACH(ptree::value_type const& p0, child.get_child("channel")){
        if(p0.first == "member" && p0.second.get<string>("<xmlattr>.type") == "interface"){
          if(fst){
            name_holder = p0.second.data();
            regex_search(name_holder, r_match, name);
            peer = r_match.str();
            fst = false;
          }
          else{
            name_holder2 = p0.second.data();
            regex_search(name_holder2, r_match, name);
            peer2 = r_match.str();
          }
        }
      }

      // set type for service matching
      BOOST_FOREACH(ptree::value_type const& pl1, pt.get_child("scenario")){
        if(pl1.first != "host" && pl1.first != "router"){
          continue;
        }
        if(pl1.second.get<string>("<xmlattr>.name") == peer){
          pType = pl1.second.get<string>("type");
        }
        if(pl1.second.get<string>("<xmlattr>.name") == peer2){
          p2Type = pl1.second.get<string>("type");
        }
      }

      // Check if nodes already exists, 
      // if so set flag and only reference by name, do not recreate
      int nNodes = nodes.GetN();
      bool pflag = false, p2flag = false;
      for(int i = 0; i < nNodes; i++){
        if(peer.compare(Names::FindName(nodes.Get(i))) == 0){
          pflag = true;
        }
        if(peer2.compare(Names::FindName(nodes.Get(i))) == 0){
          p2flag = true;
        }
      }

      if(!pflag && !p2flag){
        p2pNodes.Create(2);
        nodes.Add(p2pNodes);
        Names::Add(peer, p2pNodes.Get(0));
        Names::Add(peer2, p2pNodes.Get(1));
      }
      if(pflag && !p2flag){
        p2pNodes.Create(1);
        nodes.Add(p2pNodes);
        Names::Add(peer2, p2pNodes.Get(0));
        p2pNodes.Add(peer);
      }
      else if(!pflag && p2flag){
        p2pNodes.Create(1);
        nodes.Add(p2pNodes);
        Names::Add(peer, p2pNodes.Get(0));
        p2pNodes.Add(peer2);
      }

      // set channel parameters
      BOOST_FOREACH(ptree::value_type const& p0, child.get_child("channel")){
        if(p0.first == "parameter"){
          if(p0.second.get<string>("<xmlattr>.name") == "bw"){
            p2p.SetDeviceAttribute("DataRate", DataRateValue(stoi(p0.second.data())));
          }
          else if(p0.second.get<string>("<xmlattr>.name") == "delay"){
            p2p.SetChannelAttribute("Delay",TimeValue(MicroSeconds(stoi(p0.second.data()))));
          }
          else if(p0.second.get<string>("<xmlattr>.name") == "loss"){
            Ptr<RateErrorModel> rem = CreateObjectWithAttributes<RateErrorModel>("ErrorRate", DoubleValue(stod(p0.second.data())));
            p2p.SetDeviceAttribute("ReceiveErrorModel",PointerValue(rem));
          }
        }
      }

      p2pDevices.Add(p2p.Install(peer, peer2));

      // add internet stack if not yet created, add routing if found
      if(!pflag){
        getRoutingProtocols(pt, peer, pType);
      }
      if(!p2flag){
        getRoutingProtocols(pt, peer2, p2Type);
      }

      // Get then set addresses
      getAddresses(pt, peer, name_holder);
      Ptr<NetDevice> device = p2pDevices.Get (0);
      assignDeviceAddress(device);

      getAddresses(pt, peer2, name_holder2);
      device = p2pDevices.Get (1);
      assignDeviceAddress(device);

      if(pcap){
        nd.Add(p2pDevices);
        //p2p.EnableAsciiAll (stream);
        //p2p.EnablePcapAll (trace_prefix + "core-to-ns3");
      }

}
