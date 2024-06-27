#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <semaphore.h>
#include <time.h>
#include <stdbool.h>



//******************************************************************  
//*                      Clean up function                         * ------------------------------------------------------------------------------------------
//******************************************************************



void cleanUp(FILE *file, int stops, int *lineCounter, int *currentStop, int *onboard, int **waitingAtStop, int* skiersAtSlope,
            sem_t *semPrint, sem_t *semMutex, sem_t **semStops, sem_t *semFinalStop, sem_t *semBoarded, sem_t *semGetOff) {

      // Close the file
      fclose(file);

      // Clean up shared memory
      munmap(lineCounter, sizeof(int));
      munmap(currentStop, sizeof(int));
      munmap(onboard, sizeof(int));
      munmap(waitingAtStop, sizeof(int) * stops);
      munmap(skiersAtSlope, sizeof(int));

      // Clean up semaphores
      sem_destroy(semPrint);
      sem_destroy(semMutex);
      sem_destroy(semFinalStop);
      sem_destroy(semBoarded);
      sem_destroy(semGetOff);

      // Clean up shared memory
      munmap(semPrint, sizeof(sem_t));
      munmap(semMutex, sizeof(sem_t));
      munmap(semStops, sizeof(sem_t *) * stops);
      munmap(semFinalStop, sizeof(sem_t));
      munmap(semBoarded, sizeof(sem_t));
      munmap(semGetOff, sizeof(sem_t));

}



//******************************************************************  
//*                    Skier and Bus behaviour                     * ------------------------------------------------------------------------------------------
//******************************************************************



// SKIER BEHAVIOUR: ->
void skierBehavior(FILE *file, int skierID, int stops, int maxSkierWaitTime,
                  int *lineCounter, int *onboard, int **waitingAtStop, int* skiersAtSlope,
                  sem_t *semPrint, sem_t **semStops, sem_t *semFinalStop, sem_t *semBoarded, sem_t *semGetOff) {

      sem_t *semMyStop;

      // START -> 
      sem_wait(semPrint);
            fprintf(file, "%d: L %d: started\n", *lineCounter, skierID + 1);
            fflush(file);
            (*lineCounter)++;
      sem_post(semPrint);

      // randomly choose a stop from set interval
      srand(time(NULL) + skierID);
      int stopID = rand() % stops + 1;

      // wait for a random time from set interval and wait
      if (maxSkierWaitTime > 0) { 
            int waitTime = rand() % maxSkierWaitTime;
            usleep(waitTime);
      }

      // get the semaphore for my stop
      semMyStop = semStops[stopID-1];

      // ARRIVE AT STOP ->
      sem_wait(semPrint);
            (*waitingAtStop)[stopID]++;
            fprintf(file, "%d: L %d: arrived to %d\n", *lineCounter, skierID + 1, stopID);
            fflush(file);
            (*lineCounter)++;
      sem_post(semPrint);

      // WAIT FOR BUS ->
      sem_wait(semMyStop);

      // BOARD BUS ->
      sem_wait(semPrint);
            fprintf(file, "%d: L %d: boarding\n", *lineCounter, skierID + 1);
            fflush(file);
            (*lineCounter)++;
            (*waitingAtStop)[stopID]--;
            (*onboard)++;
      sem_post(semPrint);

      sem_post(semBoarded);

      // WAIT FOR BUS TO FINISH ->
      sem_wait(semFinalStop);

      // GET OFF BUS ->
      sem_wait(semPrint);
            fprintf(file, "%d: L %d: going to ski\n", *lineCounter, skierID + 1);
            fflush(file);
            (*lineCounter)++;
            (*onboard)--;
            (*skiersAtSlope)++;
      sem_post(semPrint);

      sem_post(semGetOff);

      exit(0);
}


// ------------------------------------------------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------------------------------------------------------------------

// BUS BEHAVIOUR: ->
void busBehavior(FILE *file, int skiers, int stops, int busCapacity, int maxBusDriveTime,
                  int *lineCounter, int *currentStop, int *onboard, int **waitingAtStop, int* skiersAtSlope,
                  sem_t *semPrint, sem_t *semMutex, sem_t **semStops, sem_t *semFinalStop, sem_t *semBoarded, sem_t *semGetOff) {
      
      bool busDriving = true;
      sem_t *semCurrentStop;
      int skiersToBoard;

      // START ->
      sem_wait(semPrint);
            fprintf(file, "%d: BUS: started\n", *lineCounter);
            fflush(file);
            (*lineCounter)++;
      sem_post(semPrint);

      while (busDriving == true){

            while(*currentStop <= stops-1){

                  // Random drive time from set interval
                  if (maxBusDriveTime > 0) {
                        int driveTime = rand() % maxBusDriveTime;
                        usleep(driveTime);   
                  }                      

                  semCurrentStop = semStops[*currentStop];

                  // ARRIVE AT STOP ->
                  sem_wait(semPrint);
                        fprintf(file, "%d: BUS: arrived to %d\n", *lineCounter, *currentStop + 1);
                        fflush(file);
                        (*lineCounter)++;
                        (*currentStop)++;
                  sem_post(semPrint);

                  // BOARDING ->
                  sem_wait(semMutex);
                        
                        if ((*waitingAtStop)[*currentStop] > busCapacity - *onboard) {
                              skiersToBoard = busCapacity - *onboard;
                        } else {
                              skiersToBoard = (*waitingAtStop)[*currentStop];
                        }

                        for (int i = 0; i < skiersToBoard; i++) {
                              sem_post(semCurrentStop);
                              sem_wait(semBoarded);
                        }     

                  sem_post(semMutex);

                  // DEPARTURE ->
                  sem_wait(semPrint);
                        fprintf(file, "%d: BUS: leaving %d\n", *lineCounter, *currentStop);
                        fflush(file);
                        (*lineCounter)++;
                  sem_post(semPrint);

            }

      // Random drive time from set interval
      if (maxBusDriveTime > 0) {
            int driveTime = rand() % maxBusDriveTime;
            usleep(driveTime);   
      }
      

      // ARRIVE AT FINAL ->
      sem_wait(semPrint);
            fprintf(file, "%d: BUS: arrived to final\n", *lineCounter);
            fflush(file);
            (*lineCounter)++;
            (*currentStop) = 0;
      sem_post(semPrint);

      // GETTING OFF ->
      sem_wait(semMutex);
            while (*onboard > 0) {
                  sem_post(semFinalStop); // Signal that each skier should get off
                  sem_wait(semGetOff); // Wait for the skier to confirm they got off
            }
            *onboard = 0;
      sem_post(semMutex);

      sem_wait(semPrint);
            fprintf(file, "%d: BUS: leaving final\n", *lineCounter);
            fflush(file);
            (*lineCounter)++;
            (*currentStop) = 0;
      sem_post(semPrint);

                        
      if (*skiersAtSlope == skiers){
            busDriving = false;
      }

      }

      // BUS TERMINATED ->
      sem_wait(semPrint);
            fprintf(file, "%d: BUS: finish\n", *lineCounter);
            fflush(file);
      sem_post(semPrint);

      exit(0);


}


//******************************************************************  
//*         Global vars with pointers to shared memory             * ------------------------------------------------------------------------------------------
//******************************************************************



int *lineCounter;
int *currentStop;
int *onboard;
int *waitingAtStop;
int *skiersAtSlope;

sem_t *semPrint;
sem_t *semMutex;
sem_t **semStops;
sem_t *semFinalStop;
sem_t *semBoarded;
sem_t *semGetOff;



//******************************************************************  
//*                  -  --  --- MAIN --- --  -                     * ------------------------------------------------------------------------------------------
//******************************************************************



int main(int argc, char *argv[]) {

      // Check if the number of arguments is correct
      if (argc != 6) { 
            printf("Usage: %s <skiers> <stops> <busCapacity> <skierWaitTime> <busDriveTime>\n", argv[0]);
            return 1;
      }

      // Get command line arguments
      int skiers           = atoi(argv[1]); // L 
      int stops            = atoi(argv[2]); // Z
      int busCapacity      = atoi(argv[3]); // K
      int maxSkierWaitTime = atoi(argv[4]); // TL - in microseconds
      int maxBusDriveTime  = atoi(argv[5]); // TB - (between two stops) in microseconds


      // Check if the input is within the correct range
      if   ((skiers > 20000 || skiers <= 0) ||                          // num. of skiers must be between 1 and 20000         skier<20000
            (stops > 10 || stops <= 0) ||                               // num. of stops must be between 1 and 10             0<stops<10
            (busCapacity > 100 || busCapacity < 10) ||                  // busCapacity must be between 10 and 100             10<=busCapacity<=100
            (maxSkierWaitTime > 10000 || maxSkierWaitTime < 0) ||       // maxSkierWaitTime must be between 0 and 10000       0<=maxSkierWaitTime<=10000
            (maxBusDriveTime > 1000 || maxBusDriveTime < 0)) {          // maxBusDriveTime must be between 0 and 1000         0<=maxBusDriveTime<=1000
      
            printf("Invalid input\n");
            return 1;
      }

      FILE *file = fopen("proj2.out", "a");
      if (file == NULL) {
            perror("fopen");
            return 1;
      }

      setvbuf(file, NULL, _IONBF, 0); // Disable buffering for the file


//******************************************************************  
//*                   Shared memory + Semaphores                   * ------------------------------------------------------------------------------------------
//******************************************************************



      // VARIABLES: (shared memory) ->
      lineCounter = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
      currentStop = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
      onboard = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
      waitingAtStop = mmap(NULL, (sizeof(int) * stops), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
      skiersAtSlope = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

      // SEMAPHORES: (shared memory) ->
      semPrint = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
      semMutex = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
      semStops = mmap(NULL, sizeof(sem_t *) * (stops), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
      semFinalStop = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
      semBoarded = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
      semGetOff = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

      
      // SEMAPHORES: (initialization) ->
      sem_init(semPrint, 1, 1);
      sem_init(semMutex, 1, 1);
      sem_init(semBoarded, 1, 0);
      sem_init(semFinalStop, 1, 0);
      sem_init(semGetOff, 1, 0);

      for (int i = 0; i < stops; i++) {
            semStops[i] = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
            sem_init(semStops[i], 1, 0); // Initialize each semaphore in the array
      }

      // Initial values for shared memory variables:
      *lineCounter = 1;       // Counts the lines in the .out file
      *currentStop = 0;       // Keeps track of what stop the bus is at
      *onboard = 0;           // How many skiers are onboard
      *skiersAtSlope = 0;     // How many skiers are 

      


//******************************************************************  
//*                           Processes                            * ------------------------------------------------------------------------------------------
//******************************************************************



      // CREATE SKIER PROCESSES

      pid_t skier_pids[skiers];
      for (int i = 0; i < skiers; i++) {
      pid_t skier_pid = fork();

      skier_pids[i] = skier_pid;

      if (skier_pid == 0) {
            // Child process (skier)
            skierBehavior(
                  file,
                  i,
                  stops,
                  maxSkierWaitTime,
                  lineCounter,
                  onboard,
                  &waitingAtStop,
                  skiersAtSlope,
                  semPrint,
                  semStops,
                  semFinalStop,
                  semBoarded,
                  semGetOff
            );
            exit(0);
      } else if (skier_pid < 0) {
            perror("fork");
            // Error handling: terminate all other processes
            for (int j = 0; j < i; j++) {
                  kill(skier_pids[j], SIGKILL);  // Terminate skier processes created before the failure
            }
            // Clean up and exit
            cleanUp(
                  file,
                  stops,
                  lineCounter,
                  currentStop,
                  onboard,
                  &waitingAtStop,
                  skiersAtSlope,
                  semPrint,
                  semMutex,
                  semStops,
                  semFinalStop,
                  semBoarded,
                  semGetOff
            );
            return 1;
            fclose(file);  // This line is unreachable because of the return statement above
      }
      }

// ------------------------------------------------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------------------------------------------------------------------

      // CREATE BUS PROCESS
      pid_t bus_pid = fork();

      if (bus_pid == 0) {
      // Child process (bus)
      busBehavior(
            file,
            skiers,
            stops,
            busCapacity,
            maxBusDriveTime,
            lineCounter,
            currentStop,
            onboard,
            &waitingAtStop,
            skiersAtSlope,
            semPrint,
            semMutex,
            semStops,
            semFinalStop,
            semBoarded,
            semGetOff
      );
      exit(0);
      } else if (bus_pid < 0) {
      perror("fork");
            // Error handling: terminate all skier processes
            kill(bus_pid, SIGKILL);  // Terminate bus process
            for (int j = 0; j < skiers; j++) {
                  kill(skier_pids[j], SIGKILL);  // Terminate all skier processes
            }
            // Clean up and exit
            cleanUp(
                  file,
                  stops,
                  lineCounter,
                  currentStop,
                  onboard,
                  &waitingAtStop,
                  skiersAtSlope,
                  semPrint,
                  semMutex,
                  semStops,
                  semFinalStop,
                  semBoarded,
                  semGetOff
            );
            return 1;
      }

      // Parent process waits for all child processes to finish
      while (wait(NULL) > 0);



//******************************************************************  
//*                           Clean-up                             * ------------------------------------------------------------------------------------------
//******************************************************************

      cleanUp(
            file,
            stops,
            lineCounter,
            currentStop,
            onboard,
            &waitingAtStop,
            skiersAtSlope,
            semPrint,
            semMutex,
            semStops,
            semFinalStop,
            semBoarded,
            semGetOff
      );

      return 0;
}