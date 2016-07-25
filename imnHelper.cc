
#include "imnHelper.h"


//credit for split methods stackoverflow: http://stackoverflow.com/questions/236129/split-a-string-in-c
namespace ns3{

NS_OBJECT_ENSURE_REGISTERED (imnHelper);

TypeId imnHelper::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::imnHelper")
    .SetParent<Object> ()
    .SetGroupName ("topology-read")
    ;
  return tid;
}

//constructor
imnHelper::imnHelper(string file_name)
{
  bracket_count = 0;
  node_count = 0;
  other_count = 0;
  p2p_count = 0;
  wifi_count = 0;
  LAN_count = 0;
  link_count = 0;
  fname = file_name;

}

imnHelper::~imnHelper ()
{

}

//Method to remove leading spaces from string
string imnHelper:: removeLeadSpaces(string s){
  string new_s;
  regex leading_spaces("[[:space:]]*(.+)");
  smatch result;
  regex_search(s,result,leading_spaces);  //search for leading spaces
  if(result[1].str().length()>0) {          //ignore empty lines
    new_s = result[1];            //write on disk the cleaned line
  }
  
  return new_s;
}

//method to split string by given delimiter and store in vector
vector<string>& imnHelper:: split(string &s, string delim, std::vector<string> &elems) {
  string item;
  size_t pos = 0;

  while ((pos = s.find(delim)) != string::npos) {
    item = s.substr(0, pos);
    if(item.compare("") != 0 )
      elems.push_back(item);
      s.erase(0, pos + delim.length());
  }
  elems.push_back(s);
  return elems;
}


vector<string> imnHelper:: split(string &s, string delim) {
  vector<string> elems;
  split(s, delim, elems);
  return elems;
}

//***********************
//READNODE
//***********************
void imnHelper::readNode(){
  string ss;
  regex node_names("[{]*n[0-9]+[}]*"); //regex to match node names of type n + number ie n1,n2,..ect
  regex number("[0-9]+");
  smatch r_match;
  int current_node_number = 0;

  imunes_stream.open(fname.c_str(), ifstream::in);
  if(!imunes_stream.is_open()){
    cerr << "Error: Could not open IMUNES file" << endl;
    return;
  }
  else{
    while(imunes_stream.good()){
      string s;
      getline(imunes_stream, s);
      string temp_line = s;
      vector<string> tokens = split(temp_line," "); //tokenize current string
      if(tokens[0].compare("node") == 0 && tokens[tokens.size() - 1].compare("{") == 0 ){ //begining of block, get node number
        
        regex_search(tokens[1],r_match,number);
        current_node_number = stoi(r_match[0]);
        //cout << "current node: " << current_node_number << endl;
        
      }
      if(s.find("node") != string::npos){
        getline(imunes_stream, ss);

          if(ss.find("router") != string::npos){
            getline(imunes_stream, ss);

            if(ss.find("router") != string::npos)
              node_count++;
            else{
              node_count++;
              other_count++;
            }
            int flag = 2;
            while(flag && imunes_stream.good()){
              string ss2;
              getline(imunes_stream, ss2);
               
              if(ss2.find("interface-peer") == string::npos){
                if(flag == 1)
                  flag = 0;
              }
              else{
                flag = 1;
                p2p_count++;
              }
            }
          }
          else if(ss.find("wlan") != string::npos ){
            int flag = 2;
            wlan_devices.push_back(current_node_number);
            while(flag && imunes_stream.good()){
              string ss2;
              getline(imunes_stream, ss2);
            
              if(ss2.find("interface-peer") == string::npos){
                if(flag == 1)
                  flag = 0;
              }
              else{
                flag = 1;
                wifi_count++;
              }
            }
          }
          else if(ss.find("hub") != string::npos){
            int flag = 2;
            csma_devices.push_back(current_node_number);
            while(flag && imunes_stream.good()){
              string ss2;
              getline(imunes_stream, ss2);

              if(ss2.find("interface-peer") == string::npos){
                if(flag == 1)
                  flag = 0;
              }
              else{
                flag = 1;
                LAN_count++;
              }
            }
          }
        }
        else if(s.find("link") != string::npos){
          link_count++;
        }
      }
    }
    p2p_count = link_count - wifi_count - LAN_count;
  }

}


