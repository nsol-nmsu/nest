// Authors:
// Armando
// John
// Andres
//
// Project description: to create an intermediate program that can take CORE
// IMUNES files and parse them into an XML format which in turn can be used
// to create an NS3 scenario or an IMN CORE file. The XML will have a schema
// to allow human input for traffic flow, routing and logging.

**Short notes:**

- imn folder must be placed inside folder ns-3.25
- Libraries for c++11 and boost must be installed
- We are currently using NetAnim to visualize the scenario output, therefore
  NetAnim must be installed:
  Qt4 (4.8 and over) is required to build NetAnim. This can be obtained using
  the following ways:

  For Debian/Ubuntu Linux distributions:
$ apt-get install qt4-dev-tools

  For Red Hat/Fedora based distribution:
$ yum install qt4
$ yum install qt4-devel

  To build NetAnim use the following commands:

$ cd netanim
$ make clean
$ qmake NetAnim.pro  (For MAC Users: qmake -spec macx-g++ NetAnim.pro)
$ make

  Note: qmake could be “qmake-qt4” in some systems
  This should create an executable named “NetAnim” in the same directory
  To run NetAnim:
$ ./NetAnim


- makefile added, 
  typing 'make configure' Copies files to their destination, configures flags and
                          compiles/links NS3 files.
  typing 'make'           Copies files to their destination, and runs ./waf to
                          compile/link NS3 files.
  typing 'make clean'     Removes <some of> the added files.

**Current developed XML Schema can be viewed here:**
https://docs.google.com/drawings/d/19wQD3N5gthTcy9LZ4ggZMSBA0dCI5UfvjWj8bXUmjLs/edit?usp=sharing

**to Generate an XML file:**
./waf --run "scratch/xml_tester --topo=imn2ns3/imn_sample_files/third.imn"

**to generate a scenario, an example syntax would be:**
./waf --run "scratch/imn_to_ns3_scenario --topo=imn2ns3/imn_sample_files/third.imn"

or

./waf --run "scratch/imn_to_ns3_scenario --topo=imn2ns3/imn_sample_files/sample4-nrlsmf.imn --traceFile=imn2ns3/imn_sample_files/sample4.ns_movements --duration=250.0"

or

./waf --run "scratch/xml_to_ns3_scenario --topo=imn2ns3/imn_sample_files/sample1.xml --traceFile=imn2ns3/imn_sample_files/sample1.ns_movements --duration=27.0"



//***************************BUGS/ERRORS/TO-DO******************************//
//**IN IMN/XML TO NS3 SCENARIO
- Currently cannot handle switch/hub to switch/hub connections
reason: NS3 doesn't not have a representation for hub/switches, currently
        using bridges. Routing bridge to bridge causes broadcast storm.

- IPv4 error when setting wireless network with other networks,
  temporary fix by changing /32 mask to /24
reason: known NS3 bug with routing protocol not yet configured for it.
        Other possible solution, setting routing individually (not explored)

- Currently has no automated application settings enabled.
reason: TO-DO




//**IN IMN TO XML GENERATOR
- Can erroneously create interfaces with RJ45 and prouters
reason: when setting p2p links, we don't check if peer nodes types.

- Can create empty interfaces/nodes
reason: custom-config can have additional settings for existing interfaces where
        our reader may incorrectly interpret them as new.
        Empty node may be a byproduct, not yet full understood.

- Can flip interface address/peer
reason: unknown, may be something more to do with custom-config additional
        settings.

- ServicePlan must be user inputed once XML intermediate file is created
reason: complexity of services/scripts




//**IN XML SCHEMA
- Event time not very useful
reason: can be used to alter simulation start and stop times but must be
        user inputted, NS3 currently does this already for stop time.

- ServicePlan unfinished.
reason: TO-DO








