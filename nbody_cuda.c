#include <stdio.h>
#include <math.h>
#include <stdlib.h> // drand48
#include <unistd.h>
#include <omp.h>
#include <cuda.h>
#include <cuda_runtime.h>

#define DUMP

#define CUDA_SAFE_CALL( __call)                                         \
  do {                                                                  \
    cudaError_t __err = __call;                                         \
    if( cudaSuccess != __err) {                                         \
      char __machinestr[64];                                            \
      int __dev;                                                        \
      cudaGetDevice(&__dev);                                            \
      gethostname(__machinestr, 64);                                    \
      fprintf(stderr, "[Machine %s - Device %d] "                       \
              "Cuda error in file '%s' in line %i : %s (%s).\n",        \
              __machinestr, __dev, __FILE__, __LINE__,                  \
              cudaGetErrorString( __err),                               \
              cudaGetErrorName( __err) );                               \
      abort();                                                          \
    }                                                                   \
  } while(0)

struct ParticleType { 
  float x, y, z;
  float vx, vy, vz; 
};

__global__ void MoveParticles(const int nParticles, struct ParticleType* const particle, const float dt) {
  int i, j;
  float Fx = 0.0, Fy = 0.0, Fz = 0.0;

  i = threadIdx.x;

  for (j = 0 ; j < nParticles ; j++) {
    if (i != j) {
      const float softening = 1e-20;

      // Newton's law of universal gravity
      const float dx = particle[j].x - particle[i].x;
      const float dy = particle[j].y - particle[i].y;
      const float dz = particle[j].z - particle[i].z;
      const float drSquared  = dx*dx + dy*dy + dz*dz + softening;
      /* const float drPower32  = pow(drSquared, 3.0/2.0); */
      const float drPower32  = drSquared*sqrtf(drSquared);

      // Calculate the net force
      Fx += dx / drPower32;
      Fy += dy / drPower32;
      Fz += dz / drPower32;
    }
  }

  // Accelerate particles in response to the gravitational force
  particle[i].vx += dt*Fx; 
  particle[i].vy += dt*Fy; 
  particle[i].vz += dt*Fz;

  __syncthreads();

  // Move particles according to their velocities
  particle[i].x  += particle[i].vx*dt;
  particle[i].y  += particle[i].vy*dt;
  particle[i].z  += particle[i].vz*dt;

}

/* void MoveParticles(const int nParticles, struct ParticleType* const particle, const float dt) { */

/*   // Loop over particles that experience force */
/*   for (int i = 0; i < nParticles; i++) {  */

/*     // Components of the gravity force on particle i */
/*     float Fx = 0, Fy = 0, Fz = 0; */
      
/*     // Loop over particles that exert force */
/*     for (int j = 0; j < nParticles; j++) { */
/*       // No self interaction */
/*       if (i != j) { */
/*         // Avoid singularity and interaction with self */
/*         const float softening = 1e-20; */

/*         // Newton's law of universal gravity */
/*         const float dx = particle[j].x - particle[i].x; */
/*         const float dy = particle[j].y - particle[i].y; */
/*         const float dz = particle[j].z - particle[i].z; */
/*         const float drSquared  = dx*dx + dy*dy + dz*dz + softening; */
/*         /\* const float drPower32  = pow(drSquared, 3.0/2.0); *\/ */
/*         const float drPower32  = drSquared*sqrtf(drSquared); */

/*         // Calculate the net force */
/*         Fx += dx / drPower32; */
/*         Fy += dy / drPower32; */
/*         Fz += dz / drPower32; */
/*       } */

/*     } */

/*     // Accelerate particles in response to the gravitational force */
/*     particle[i].vx += dt*Fx;  */
/*     particle[i].vy += dt*Fy;  */
/*     particle[i].vz += dt*Fz; */
/*   } */

/*   // Move particles according to their velocities */
/*   // O(N) work, so using a serial loop */
/*   for (int i = 0 ; i < nParticles; i++) {  */
/*     particle[i].x  += particle[i].vx*dt; */
/*     particle[i].y  += particle[i].vy*dt; */
/*     particle[i].z  += particle[i].vz*dt; */
/*   } */
/* } */

void dump_1_part(int step, FILE *f, int i, struct ParticleType* particle, struct ParticleType* particle_d) {
  if (f == NULL) return;

  CUDA_SAFE_CALL(cudaMemcpy(&particle[i], &particle_d[i], sizeof(struct ParticleType), cudaMemcpyDeviceToHost));

  fprintf(f, "%d, %e %e %e %e %e %e\n",
          step, particle[i].x, particle[i].y, particle[i].z,
          particle[i].vx, particle[i].vy, particle[i].vz);
}

void dump(int iter, int nParticles, struct ParticleType* particle, struct ParticleType* particle_d)
{
  char filename[64];
  snprintf(filename, 64, "results/output_%d.txt", iter);

  CUDA_SAFE_CALL(cudaMemcpy(particle, particle_d, nParticles * sizeof(struct ParticleType), cudaMemcpyDeviceToHost));

  FILE *f;
  f = fopen(filename, "w+");

  int i;
  for (i = 0; i < nParticles; i++)
    {
      fprintf(f, "%e %e %e %e %e %e\n",
              particle[i].x, particle[i].y, particle[i].z,
              particle[i].vx, particle[i].vy, particle[i].vz);
    }

  fclose(f);
}

int main(const int argc, const char** argv)
{

  // Problem size and other parameters
  const int nParticles = (argc > 1 ? atoi(argv[1]) : 16384);
  // Duration of test
  const int nSteps = (argc > 2)?atoi(argv[2]):10;
  // Particle propagation time step
  const float dt = 0.0005f;
  float runtime = 0.0f;

  // File to follow 1 particle
  FILE *file;
  file = fopen("results/one_particle.txt", "w");

  int size = nParticles*sizeof(struct ParticleType);
  struct ParticleType* particle = (struct ParticleType*) malloc(size);
  struct ParticleType* particle_d;
  CUDA_SAFE_CALL(cudaMalloc((void **) &particle_d, size));

  // Initialize random number generator and particles
  srand48(0x2020);

  int i;
  for (i = 0; i < nParticles; i++)
    {
      particle[i].x =  2.0*drand48() - 1.0;
      particle[i].y =  2.0*drand48() - 1.0;
      particle[i].z =  2.0*drand48() - 1.0;
      particle[i].vx = 2.0*drand48() - 1.0;
      particle[i].vy = 2.0*drand48() - 1.0;
      particle[i].vz = 2.0*drand48() - 1.0;
    }

  // Copy the table on the device
  CUDA_SAFE_CALL(cudaMemcpy(particle_d, particle, size, cudaMemcpyHostToDevice));

  // Perform benchmark
  printf("\nPropagating %d particles using 1 thread...\n\n", 
	 nParticles
	 );
  double rate = 0, dRate = 0; // Benchmarking data
  const int skipSteps = 3; // Skip first iteration (warm-up)
  printf("\033[1m%5s %10s %10s %8s\033[0m\n", "Step", "Time, s", "Interact/s", "GFLOP/s"); fflush(stdout);

  for (int step = 1; step <= nSteps; step++) {

    const double tStart = 0; // Start timing
    MoveParticles<<<1, size>>>(nParticles, particle_d, dt);
    const double tEnd = 1; // End timing

    runtime += tEnd - tStart;
    const float HztoInts   = ((float)nParticles)*((float)(nParticles-1)) ;
    const float HztoGFLOPs = 20.0*1e-9*((float)(nParticles))*((float)(nParticles-1));

    if (step > skipSteps) { // Collect statistics
      rate  += HztoGFLOPs/(tEnd - tStart); 
      dRate += HztoGFLOPs*HztoGFLOPs/((tEnd - tStart)*(tEnd-tStart)); 
    }

    printf("%5d %10.3e %10.3e %8.1f %s\n", 
	   step, (tEnd-tStart), HztoInts/(tEnd-tStart), HztoGFLOPs/(tEnd-tStart), (step<=skipSteps?"*":""));
    fflush(stdout);

#ifdef DUMP
    dump(step, nParticles, particle, particle_d);
    dump_1_part(step, file, 0, particle, particle_d);
#endif
    particle[0] = step * 1.0;
  }

  fclose(file);
  rate/=(double)(nSteps-skipSteps); 
  dRate=sqrt(dRate/(double)(nSteps-skipSteps)-rate*rate);
  printf("-----------------------------------------------------\n");
  printf("\033[1m%s %4s \033[42m%10.1f +- %.1f GFLOP/s\033[0m\n",
	 "Average performance:", "", rate, dRate);
  printf("-----------------------------------------------------\n");
  printf("Total time = %f \n", runtime);
  printf("* - warm-up, not included in average\n\n");
  free(particle);
  cudaFree(particle_d);
}
