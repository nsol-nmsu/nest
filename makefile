CPATH = ../src/topology-read/imn_reader/
IMNFILES = imnHelper.cc imnHelper.h imnNode.cc imnNode.h imnLink.cc imnLink.h xmlGenerator.cc xmlGenerator.h

all: update

update: copy
	cd .. && ./waf

configure: copy
	cd .. && CXXFLAGS="-std=c++11" ./waf configure

copy: makeDir
	cp ns3_imn_parser.cc ../scratch/
	cp xml_tester.cc ../scratch/
	cp $(IMNFILES) $(CPATH)
	mv -vn ../src/topology-read/wscript ../src/topology-read/wscript.old
	cp wscript ../src/topology-read/

makeDir:
	mkdir -vp ../src/topology-read/imn_reader

clean:
	rm -f ../scratch/ns3_imn_parser.cc 
	rm -rf ../src/topology-read/imn_reader

