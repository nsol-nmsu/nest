#!/usr/bin/python
import os, sys, getopt

##find path to program to run
def find(name, path):
  for root, dirs, files in os.walk(path):
    if name in files:
      return os.path.join(root, name)

def addOptions(topo, traceDir, ns2_file, duration):
  run_command = "scratch/core_to_ns3_scenario"
  if topo != "":
    run_command = run_command + " --topo=imn2ns3/imn_sample_files/" + topo
  if traceDir != "":
    run_command = run_command + " --traceDir=core2ns3_Logs/"
  if ns2_file != "":
    run_command = run_command + " --ns2=imn2ns3/imn_sample_files/" + ns2_file
  if duration != "":
    run_command = run_command + " --duration=" + duration
  run_command = '"' + run_command + '"'
  
  waf_command = './waf --run ' + run_command
  return waf_command
      
def main(argv):
  f = find("core_to_ns3_scenario.cc", os.path.expanduser('~'))
  path_to_file = f.replace('/imn2ns3/core_to_ns3_scenario.cc','')
  os.chdir(path_to_file)
  
  topo = ''
  trace = ''
  ns2_file = ''
  duration = ''

  try:
    opts, args = getopt.getopt(argv, 'ht:n:d:l', ["help", "topo=", "ns2_file=", "duration=", "trace"])
  except getopt.GetoptError:
    print "xml_to_ns3_pipeline.py --topo/-t=<file.xml> --ns2_file/-n=<file_name> --duration/-d=<time> --trace/-l"
    sys.exit(2)
  for opt, arg in opts:
    if opt in ("-h", "--help"):
      print "xml_to_ns3_pipeline.py --topo/-t=<file.xml> --ns2_file/-n=<file_name> --duration/-d=<time> --trace/-l"
      sys.exit()
    elif opt in ("-t" ,"--topo"):
      topo = arg
    elif opt in ("-n", "--ns2_file"):
      ns2_file = arg
    elif opt in ("-d" ,"--duration"):
      duration = arg
    elif opt in ("-l", "--trace"):
      trace = "yes"

  if topo == "":
    print "There was no xml file given as input, exiting program"
    sys.exit()
  ##print addOptions(topo,trace,ns2_file,duration)
  os.system(addOptions(topo,trace,ns2_file,duration))

  ##open netAnim
  f = find("NetAnim", os.path.expanduser('~'))
  path_to_file = f.replace('NetAnim','')
  os.chdir(path_to_file)
  os.system('./NetAnim')

##run script 
if __name__ == "__main__":
   main(sys.argv[1:])


"""
**to generate a scenario, an example syntax would be:**
./waf --run "scratch/imn_to_ns3_scenario --topo=imn2ns3/imn_sample_files/third.imn"

or

./waf --run "scratch/imn_to_ns3_scenario --topo=imn2ns3/imn_sample_files/sample4-nrlsmf.imn --ns2=imn2ns3/imn_sample_files/sample4.ns_movements --duration=250.0"

or

./waf --run "scratch/xml_to_ns3_scenario --topo=imn2ns3/imn_sample_files/sample1.xml --ns2=imn2ns3/imn_sample_files/sample1.ns_movements --duration=27.0"

or

./waf --run "scratch/core_to_ns3_scenario --topo=imn2ns3/imn_sample_files/WideAreaNetwork3.xml --traceDir=core2ns3_Logs/ --duration=30.0"
"""
