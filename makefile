HPATH = ../src/topology-read/imn_reader/
<<<<<<< HEAD
IMNHELPERFILES = imnHelper.cc imnHelper.h imnNode.cc imnNode.h imnLink.cc imnLink.h xmlGenerator.cc xmlGenerator.h
NSFILES = ns3_imn_parser.cc xml_tester.cc xml_to_ns3_scenario.cc
=======
IMNHELPERFILES = imnHelper.cc imnHelper.h imnNode.cc imnNode.h imnLink.cc imnLink.h xmlGenerator.cc xmlGenerator.h 
NSFILES = ns3_imn_parser.cc xml_tester.cc
>>>>>>> 4e4d178c36f888b51f52981b21b27452b7739f75
SPATH = ../scratch/

all: update

update: copy
	cd .. && ./waf

configure: copy
	cd .. && CXXFLAGS="-std=c++11" ./waf configure

copy: makeDir
	cp $(NSFILES) $(SPATH)
	cp $(IMNHELPERFILES) $(HPATH)
	mv -vn ../src/topology-read/wscript ../src/topology-read/wscript.old
	cp wscript ../src/topology-read/

makeDir:
	mkdir -vp ../src/topology-read/imn_reader

clean:
	rm -f ../scratch/ns3_imn_parser.cc 
	rm -rf ../src/topology-read/imn_reader

