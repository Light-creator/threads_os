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
  
  // CRITICAL_SECTION cs;
  // CONDITION_VARIABLE cond;
  // HANDLE mutex;

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

    // printf("[+] Start Eating: %d\n", idx+1);
    
    Sleep(state.time_to_sleep);
    EnterCriticalSection(&state.cs);
    // printf("[+] CountEating: %d | Prev: %c | Next: %c\n", state.count_eating, state.phil_states[idx_prev], state.phil_states[idx_next]);
    
    state.phil_states[idx] = 'H';
    while(state.count_eating == PHIL_COUNT-1 || !can_eat(idx)) {
      SleepConditionVariableCS(&state.cond, &state.cs, INFINITE);
    }

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

    LeaveCriticalSection(&state.cs);

    Sleep(state.time_to_sleep);

    EnterCriticalSection(&state.cs);
    
    state.curr_time += state.time_to_sleep;
    state.count_eating--;
    state.phil_states[idx] = 'T';
    printf("%d:%d:E->T\n", state.curr_time, idx+1, state.phil_states[idx]);
    
    uint16_t idx_prev = (uint16_t)(idx+PHIL_COUNT-1)%PHIL_COUNT;
    uint16_t idx_next = (uint16_t)(idx+1)%PHIL_COUNT;

    if(state.phil_states[idx_prev] == 'H' && can_eat(idx_prev)) WakeConditionVariable(&state.cond);
    if(state.phil_states[idx_next] == 'H' && can_eat(idx_next)) WakeConditionVariable(&state.cond);

    LeaveCriticalSection(&state.cs);
  }

  return 0;
}


void init_state(int total_time, int time_to_sleep) {
  state.total_time = total_time;
  state.time_to_sleep = time_to_sleep;

  InitializeCriticalSection(&state.cs);
  InitializeConditionVariable(&state.cond);
  
  for(int i=0; i<PHIL_COUNT; i++) 
    state.phil_states[i] = 'T';

  state.mutex = CreateMutex(NULL, FALSE, NULL);
}

int main(int argc, char** argv) {
  init_state(atoi(argv[1]), atoi(argv[2]));
  
  int idxs[PHIL_COUNT];
  for(int i=0; i<PHIL_COUNT; i++) {
    idxs[i] = i;
    state.ths[i] = CreateThread(NULL, 0, start_eating, &idxs[i], 0, NULL);
  }

  WaitForMultipleObjects(PHIL_COUNT, state.ths, TRUE, INFINITE);
    
  for(int i=0; i<PHIL_COUNT; i++)
    CloseHandle(state.ths[i]);

  DeleteCriticalSection(&state.cs);

  return 0;
}
