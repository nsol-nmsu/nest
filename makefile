all: update

update: makeDir makeDir
	cp ns3_imn_parser.cc ../scratch/
	cp imnHelper.cc ../src/topology-read/imn_reader/
	cp imnHelper.h ../src/topology-read/imn_reader/
	mv -vn ../src/topology-read/wscript ../src/topology-read/wscript.old
	cp wscript ../src/topology-read/

makeDir:
	mkdir -vp ../src/topology-read/imn_reader

clean:
	rm -f ../scratch/ns3_imn_parser.cc 
	rm -rf ../src/topology-read/imn_reader

run:
	cd .. && CXXFLAGS="-std=c++11" ./waf configure
