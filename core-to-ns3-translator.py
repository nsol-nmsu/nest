#!/usr/bin/python
import os, sys, getopt

##find path to program to run
def find(name, path):
  for root, dirs, files in os.walk(path):
    if name in files:
      return os.path.join(root, name)

def addOptions(topo, traceDir, ns2, duration, apps, pcap):
  run_command = "scratch/core_to_ns3_scenario"
  if topo != "":
    run_command = run_command + " --topo=imn2ns3/" + topo
  if apps != "":
    run_command = run_command + " --apps=imn2ns3/" + apps
  if traceDir != "":
    run_command = run_command + " --traceDir=" + traceDir
  if ns2 != "":
    run_command = run_command + " --ns2=imn2ns3/" + ns2
  if duration != "":
    run_command = run_command + " --duration=" + duration
  if pcap != "":
    run_command = run_command + " --pcap=true"
  run_command = '"' + run_command + '"'
  
  waf_command = './waf --run ' + run_command

  print "\n" + waf_command + "\n"
  return waf_command
      
def main(argv):
  f = find("core_to_ns3_scenario.cc", os.path.expanduser('~'))
  path_to_file = f.replace('/imn2ns3/core_to_ns3_scenario.cc','')
  os.chdir(path_to_file)
  
  topo = ''
  trace = ''
  ns2 = ''
  duration = ''
  apps = ''
  pcap = "false"

  try:
    opts, args = getopt.getopt(argv, 'h:t:n:d:l:a:p', ["help", "topo=", "ns2=", "duration=", "trace=", "apps=", "pcap"])
  except getopt.GetoptError:
    print "core-to-ns3-translator.py -t <file/path/in/imn2ns3> -n <file/path/in/imn2ns3> -d <time.float> -l <path/to/folder/> -a <file/path/in/imn2ns3> -p"
    sys.exit(2)
  for opt, arg in opts:
    if opt in ("-h", "--help"):
      print "core-to-ns3-translator.py -t <file/path/in/imn2ns3> -n <file/path/in/imn2ns3> -d <time.float> -l <path/to/folder/> -a <file/path/in/imn2ns3> -p"
      sys.exit()
    elif opt in ("-t" ,"--topo"):
      topo = arg
    elif opt in ("-a" ,"--apps"):
      apps = arg
    elif opt in ("-n", "--ns2"):
      ns2 = arg
    elif opt in ("-d" ,"--duration"):
      duration = arg
    elif opt in ("-l", "--trace"):
      trace = arg
    elif opt in ("-p" ,"--pcap"):
      pcap = "true"

  if topo == "":
    print "There was no xml file given as input, exiting program"
    sys.exit()
  ##print addOptions(topo,trace,ns2_file,duration)
  os.system(addOptions(topo,trace,ns2,duration,apps,pcap))

  ##open netAnim
  f = find("NetAnim", os.path.expanduser('~'))
  path_to_file = f.replace('NetAnim','')
  os.chdir(path_to_file)
  os.system('./NetAnim &')

##run script 
if __name__ == "__main__":
   main(sys.argv[1:])


"""
**to generate a scenario, an example syntax would be:**
./waf --run "scratch/core_to_ns3_scenario --topo=imn2ns3/CORE-XML-files/sample1.xml --apps=imn2ns3/apps-files/sample1-apps.xml --ns2=imn2ns3/NS2-mobility-files/sample1.ns_movements --duration=27.0 --pcap=true --traceDir=core2ns3_Logs/"
"""
