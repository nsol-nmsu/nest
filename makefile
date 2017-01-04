UTILS = utils/LatLong-UTMconversion.cpp utils/LatLong-UTMconversion.h utils/constants.h utils/core-to-ns3-helper.cc utils/core-to-ns3-helper.h
NSFILES = core_to_ns3_scenario.cc
LPATH = ../src/topology-read/UTM_conversion/
SPATH = ../scratch/

all: update

update: copy
	cd .. && ./waf

configure: copy
	cd .. && CXXFLAGS="-std=c++11 -g" ./waf configure

copy: makeDir
	cp $(NSFILES) $(SPATH)
	cp $(UTILS) $(LPATH)
	mv -vn ../src/topology-read/wscript ../src/topology-read/wscript.old
	cp utils/wscript ../src/topology-read/

makeDir:
	mkdir -vp ../src/topology-read/UTM_conversion
	mkdir -vp ../core2ns3_Logs

clean:
	rm -f ../scratch/core_to_ns3_scenario.cc
	rm -rf ../src/topology-read/UTM_conversion
	rm -rf ../src/topology-read/wscript
	mv ../src/topology-read/wscript.old ../src/topology-read/wscript

cleanLogs:
	rm -rf ../core2ns3_Logs/*
