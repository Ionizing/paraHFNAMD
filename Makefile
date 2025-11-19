EXE = HFNAMD

all: $(EXE) efieldjs2txt.x

$(EXE): src/$@
	cd src && $(MAKE) && cp $(EXE) ..

efieldjs2txt.x: src/efieldjs2txt/$@
	cd src/efieldjs2txt && $(MAKE) && cp efieldjs2txt.x ../..

clean: 
	cd src && $(MAKE) clean
	rm -f HFNAMD

veryclean:
	cd src && $(MAKE) veryclean
	rm -f HFNAMD
