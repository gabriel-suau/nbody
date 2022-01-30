nbody: nbody.c
	gcc nbody.c -o nbody -lm -fopenmp -O3

clean:
	rm -f nbody

