EXE = HFNAMD

all:
	cd ./src && $(MAKE)

clean: 
	cd src && $(MAKE) clean && cd ..
	rm -f HFNAMD

veryclean:
	cd src && $(MAKE) veryclean && cd ..
	rm -f HFNAMD
