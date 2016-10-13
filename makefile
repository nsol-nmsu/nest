IMNHELPERFILES = imnHelper.cc imnHelper.h imnNode.cc imnNode.h imnLink.cc imnLink.h xmlGenerator.cc xmlGenerator.h
LATLONGUTM = utils/LatLong-UTMconversion.cpp utils/LatLong-UTMconversion.h utils/constants.h
NSFILES = imn_to_ns3_scenario.cc xml_to_ns3_scenario.cc core_to_ns3_scenario.cc xml_tester.cc
HPATH = ../src/topology-read/imn_reader/
LPATH = ../src/topology-read/UTM_conversion/
SPATH = ../scratch/

all: update

update: copy
	cd .. && ./waf

configure: copy
	cd .. && CXXFLAGS="-std=c++11" ./waf configure

copy: makeDir
	cp $(NSFILES) $(SPATH)
	cp $(IMNHELPERFILES) $(HPATH)
	cp $(LATLONGUTM) $(LPATH)
	mv -vn ../src/topology-read/wscript ../src/topology-read/wscript.old
	cp utils/wscript ../src/topology-read/

makeDir:
	mkdir -vp ../src/topology-read/imn_reader
	mkdir -vp ../src/topology-read/UTM_conversion

clean:
	rm -f ../scratch/imn_to_ns3_scenario.cc
	rm -f ../scratch/xml_to_ns3_scenario.cc
	rm -f ../scratch/core_to_ns3_scenario.cc
	rm -f ../scratch/xml_tester.cc
	rm -rf ../src/topology-read/imn_reader
	rm -rf ../src/topology-read/UTM_conversion
	rm -rf ../src/topology-read/wscript
	mv ../src/topology-read/wscript.old ../src/topology-read/wscript

