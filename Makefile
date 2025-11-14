EXE = HFNAMD

all: $(EXE)

$(EXE):
	cd ./src && $(MAKE)

clean: 
	cd src && $(MAKE) clean && cd ..
	rm -f HFNAMD

veryclean:
	cd src && $(MAKE) veryclean && cd ..
	rm -f HFNAMD
