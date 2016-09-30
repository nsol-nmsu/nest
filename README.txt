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



