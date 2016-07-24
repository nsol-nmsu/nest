all: update

update: makeDir
	mv ns3_imn_parser.cc ../scratch/
	mv imnHelper.cc ../src/topology-read/imn_reader/
	mv imnHelper.h ../src/topology-read/imn_reader/

makeDir:
	mkdir ../src/topology-read/imn_reader

clean:
	rm -f ../scratch/ns3_imn_parser.cc 
	rm -rf ../src/topology-read/imn_reader
