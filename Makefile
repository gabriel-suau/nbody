all:
	make nbody
	make nbody_omp
	make nbody_acc

nbody: nbody.c
	gcc nbody.c -o nbody -lm -fopenmp -fopenacc -O3

nbody_omp: nbody_omp.c
	gcc nbody_omp.c -o nbody_omp -lm -fopenmp -fopenacc -O3

nbody_acc: nbody_acc.c
	gcc nbody_acc.c -o nbody_acc -lm -fopenmp -fopenacc -O3

clean:
	rm -f nbody nbody_omp nbody_acc
