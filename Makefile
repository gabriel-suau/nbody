CC := gcc -fopenmp -openacc
# CC := nvcc -acc -fopenmp

all:
	make nbody
	make nbody_omp
	make nbody_acc

nbody: nbody.c
	$(CC) nbody.c -o nbody -lm -O3

nbody_omp: nbody_omp.c
	$(CC) nbody_omp.c -o nbody_omp -lm -O3

nbody_acc: nbody_acc.c
	$(CC) nbody_acc.c -o nbody_acc -lm -O3

clean:
	rm -f nbody nbody_omp nbody_acc
