#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <stdint.h>

#define PHIL_COUNT 5

typedef struct state_t_ {
  int total_time;
  int curr_time;
  uint32_t time_to_sleep;

  char phil_states[PHIL_COUNT];
  int count_eating;
  
  // Second solution
  HANDLE forks[PHIL_COUNT];
  HANDLE eating_semaphore;
  
  // First solution
  CRITICAL_SECTION cs;
  CONDITION_VARIABLE cond;
  HANDLE mutex;
  
  int waiting_queue[PHIL_COUNT];

  HANDLE ths[PHIL_COUNT];
} state_t;

state_t state;

BOOL can_eat(int idx) {
  uint16_t idx_prev = (uint16_t)(idx+PHIL_COUNT-1)%PHIL_COUNT;
  uint16_t idx_next = (uint16_t)(idx+1)%PHIL_COUNT;
  return (state.phil_states[idx_prev] != 'E' && state.phil_states[idx_next] != 'E');
}
 
DWORD WINAPI start_eating(LPVOID lp) {
  int idx = *(int*)lp;

  uint16_t idx_prev = (uint16_t)(idx+PHIL_COUNT-1)%PHIL_COUNT;
  uint16_t idx_next = (uint16_t)(idx+1)%PHIL_COUNT;
  while(state.total_time > state.curr_time) {

    Sleep(state.time_to_sleep); // Tinking
    EnterCriticalSection(&state.cs);
  
    state.phil_states[idx] = 'H';
    while((!can_eat(idx) || state.waiting_queue[0] != idx) && state.total_time > state.curr_time) {
      int is_in_queue = 0;
      for(int i=0; i<PHIL_COUNT; i++) {
        if(state.waiting_queue[i] == idx) {
          is_in_queue = 1;
          break;
        }
      }

      if(!is_in_queue) {
        for(int i=0; i<PHIL_COUNT; i++) {
          if(state.waiting_queue[i] == -1) {
            state.waiting_queue[i] = idx;
            break;
          }
        }
      }

      SleepConditionVariableCS(&state.cond, &state.cs, INFINITE);
    } 

    // while(state.count_eating == PHIL_COUNT-1 || !can_eat(idx)) {
    //   SleepConditionVariableCS(&state.cond, &state.cs, INFINITE);
    // }

    if(state.total_time <= state.curr_time || 
        (state.count_eating > 0 && state.curr_time+state.time_to_sleep*state.count_eating >= state.total_time)) {
      WakeConditionVariable(&state.cond);
      LeaveCriticalSection(&state.cs);
      break;
    }

    // Start eating
    printf("%d:%d:T->E\n", state.curr_time, idx+1, state.phil_states[idx]);
    state.count_eating++;
    state.phil_states[idx] = 'E';

    // Delete from queue
    for(int i=0; i<PHIL_COUNT-1; i++) 
      state.waiting_queue[i] = state.waiting_queue[i+1];
    state.waiting_queue[PHIL_COUNT-1] = -1;

    LeaveCriticalSection(&state.cs);

    Sleep(state.time_to_sleep); // Eating

    EnterCriticalSection(&state.cs);
  
    state.curr_time += state.time_to_sleep;
    state.count_eating--;
    state.phil_states[idx] = 'T';
    printf("%d:%d:E->T\n", state.curr_time, idx+1, state.phil_states[idx]);
  
    if(state.waiting_queue[0] != -1) 
      WakeConditionVariable(&state.cond);

    // uint16_t idx_prev = (uint16_t)(idx+PHIL_COUNT-1)%PHIL_COUNT;
    // uint16_t idx_next = (uint16_t)(idx+1)%PHIL_COUNT;

    // if(state.phil_states[idx_prev] == 'H' && can_eat(idx_prev)) WakeConditionVariable(&state.cond);
    // if(state.phil_states[idx_next] == 'H' && can_eat(idx_next)) WakeConditionVariable(&state.cond);

    LeaveCriticalSection(&state.cs);
  }

  return 0;
}

void acquire_forks(int idx) {
  uint16_t idx_next = (uint16_t)(idx+1)%PHIL_COUNT;
  
  if(idx < idx_next) {
    WaitForSingleObject(state.forks[idx], INFINITE);
    WaitForSingleObject(state.forks[idx_next], INFINITE);
  } else {
    WaitForSingleObject(state.forks[idx_next], INFINITE);
    WaitForSingleObject(state.forks[idx], INFINITE);
  }
}

void release_forks(int idx) {
  uint16_t idx_next = (uint16_t)(idx+1)%PHIL_COUNT;
  ReleaseMutex(state.forks[idx]);
  ReleaseMutex(state.forks[idx_next]);
}

// DWORD WINAPI start_eating(LPVOID lp) {
//   int idx = *(int*)lp;
// 
//   while(state.total_time > state.curr_time) {
//     WaitForSingleObject(state.eating_semaphore, INFINITE);
//    
//     acquire_forks(idx);
//     if(state.total_time <= state.curr_time+state.count_eating*state.time_to_sleep) {
//       release_forks(idx);
//       break;
//     }
//
//     printf("%d:%d:T->E\n", state.curr_time, idx+1, state.phil_states[idx]);
//     state.count_eating++;
//
//     Sleep(state.time_to_sleep);
//
//     state.curr_time += state.time_to_sleep;
//     printf("%d:%d:E->T\n", state.curr_time, idx+1, state.phil_states[idx]);
//     state.count_eating--;
//     release_forks(idx);
//
//     ReleaseSemaphore(state.eating_semaphore, 1, NULL);
//   }
//
//   return 0;
// }

void init_state(int total_time, int time_to_sleep) {
  state.total_time = total_time;
  state.time_to_sleep = time_to_sleep;

  for(int i=0; i<PHIL_COUNT; i++) 
    state.phil_states[i] = 'T';

  
  // First Solution
  InitializeCriticalSection(&state.cs);
  InitializeConditionVariable(&state.cond);
  
  memset(state.waiting_queue, -1, PHIL_COUNT*sizeof(int));
  state.waiting_queue[0] = 0;
   
  // Second Solution
  for(int i=0; i<PHIL_COUNT; i++)
    state.forks[i] = CreateMutex(NULL, FALSE, NULL);

  state.eating_semaphore = CreateSemaphore(NULL, PHIL_COUNT-1, PHIL_COUNT-1, NULL);
}

int main(int argc, char** argv) {
  init_state(atoi(argv[1]), atoi(argv[2]));
  
  int idxs[PHIL_COUNT];
  for(int i=0; i<PHIL_COUNT; i++) {
    idxs[i] = i;
    state.ths[i] = CreateThread(NULL, 0, start_eating, &idxs[i], 0, NULL);
  }

  WaitForMultipleObjects(PHIL_COUNT, state.ths, TRUE, INFINITE);
    
  for(int i=0; i<PHIL_COUNT; i++) {
    CloseHandle(state.ths[i]);
    CloseHandle(state.forks[i]);
  }
  
  CloseHandle(state.eating_semaphore);
  DeleteCriticalSection(&state.cs);

  return 0;
}
