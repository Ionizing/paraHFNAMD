EXE = HFNAMD

all: $(EXE)

$(EXE):
	cd ./src && $(MAKE) && cp -f $(EXE) .. && cd ..

clean: 
	cd src && $(MAKE) clean && cd ..

veryclean:
	cd src && $(MAKE) veryclean && cd ..
