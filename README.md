# CORE To NS-3 Translator #
The CORE to NS-3 translator aims to provide a quick and easy way to validate a CORE emulation with an NS-3 simulation. 
It is an intermediate program that can take a CORE XML file and dynamically create an NS3 scenario.

## 1) Build / Install ##

You need BOOST and C++11 libraries to compile and use core to ns-3 translator.

The program folder must exists inside the ns-3.25 folder to properly install using only the makefile.

We do encourage using NetAnim to visualize the scenario output. Build instructions can be found in
[https://www.nsnam.org/wiki/NetAnim_3.107](Link URL)

to use our program, enter the imn2ns3 folder and run the 'make configure'.

```
#!terminal

cd imn2ns3
make configure
```


Once waf has finished, run 'make', this will place files where they need to be.
```
#!terminal

make
```

Here is a makefile short description:
 
* typing `make configure` Copies files to their destination, configures flags and compiles/links NS3 files.
* typing `make`           Copies files to their destination, and runs ./waf to compile/link NS3 files.
* typing `make clean`     Removes <some of> the added files.
* typing `make cleanLogs` Removes all files and directories inside core2ns3_Logs folder

## 2) Execute ##

Once all files have been placed and compiled, return to ns3.25 directory and run
the following sample for confirmation:

```
#!terminal

cd ..
./waf --run "scratch/core_to_ns3_scenario --topo=imn2ns3/CORE-XML-files/sample1.xml"
```


Program also supports generating traffic flow using a simple XML Schema.
Examples of the schema exists inside the apps-files/ directory.

Program also supports NS2 mobility scripts.
*Note:* Node ID for NS3 nodes will differ as they are assigned at the time  they are
created. The NS2 script used for CORE will have be to altered to mirror this change.
A map is given every time the topology finishes being created to help identify the changes.
Currently this can only be done by the user.

The following is a list of the commands supported by the core to ns-3 translator:

"topo" Path to intermediate topology file

```
#!terminal

--topo=imn2ns3/CORE-XML-files/sample1.xml
```

"apps" Path to application generator file

```
#!terminal

--apps=imn2ns3/apps-files/sample1-apps.xml
```

  "ns2" Ns2 mobility script file

```
#!terminal

--ns2=imn2ns3/NS2-mobility-files/sample1.ns_movements
```

"duration" Duration of Simulation

```
#!terminal

--duration=27.0
```

  "pcap" Enable pcap files"

```
#!terminal

--pcap=true
```

"traceDir" Directory in which to store trace files

```
#!terminal

--traceDir=core2ns3_Logs/
```


An example of this will look is as follows
```
#!terminal

./waf --run "scratch/core_to_ns3_scenario --topo=imn2ns3/CORE-XML-files/sample1.xml --apps=imn2ns3/apps-files/sample1-apps.xml --ns2=imn2ns3/NS2-mobility-files/sample1.ns_movements --duration=27.0 --pcap=true --traceDir=core2ns3_Logs/"
```
## ***LIMITATIONS/TO-DOS*** ##

* Network error when setting wireless nodes with submask of all ones.

  - Current implemetation assumes nodes are in an ad-hoc network, trying to
    get subnet-directed broadcast address with an all-ones netmask will
    cause errors.

* Currently implements all hubs and switches as bridge devices.

  - NS3 doesn't not have a representation for hub/switches, currently
    using bridges.

* Currently has limited application settings enabled.

  - Routing protocols available in CORE may not be available in NS3.25.
  - GlobalRoutingProtocol commonly used in NS3 is not suitable for wireless
    nodes therefore cannot be used to link all nodes correctly.
  - Reading script files to build a route is not supported.