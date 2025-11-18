CXX = g++
CXXFLAGS = -g -O2
LDFLAGS = -L../quickjs -lqjs -lpthread -ldl

efieldjs2txt.x: efieldjs2txt.o
	$(CXX) -o $@ $< $(CXXFLAGS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) -c -o $@ $< $(CXXFLAGS) $(LDFLAGS)

clean:
	rm -rf *.o *.x
