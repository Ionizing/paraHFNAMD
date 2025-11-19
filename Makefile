EXE = HFNAMD

all: $(EXE) efieldjs2txt.x

$(EXE):
	cd src && $(MAKE) && cp $(EXE) ..
	cd src/efieldjs2txt && $(MAKE) && cp efieldjs2txt.x ../..

clean: 
	cd src && $(MAKE) clean
	rm -f HFNAMD

veryclean:
	cd src && $(MAKE) veryclean
	rm -f HFNAMD
