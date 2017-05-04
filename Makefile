all: transience.o
	g++ -o transience transience.o -lsndfile
transience.o:
	g++ -c transience.cc
clean:
	rm -f *.o transience
