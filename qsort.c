#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#define MAX_TASKS 32768

/* --------- Structs --------- */
typedef struct task_t_ {
  void (*func)(void*);
  void* arg;
} task_t;

typedef struct queue_t_ {
  task_t buf[MAX_TASKS];
  int front;
  int back;

  int count;

  pthread_mutex_t m;
  pthread_cond_t cond_full;
  pthread_cond_t cond_empty;
} queue_t;

typedef struct thread_pool_t_ {
  pthread_t* ths;
  queue_t tasks;

  int sz;

  int executing_task_counter;
  int is_shutdown;
} thread_pool_t;

typedef struct sort_arg_t_ {
  int left, right;
  int* arr;
} sort_arg_t;

typedef struct state_t_ {
  clock_t start;
  clock_t end;
} state_t;

state_t state;
thread_pool_t pool;

/* --------- Single thread --------- */

void print_arr(int* arr, int sz) {
  for(int i=0; i<sz; i++) printf("%d ", arr[i]);
  printf("\n");
}

void swap(int* x, int* y) {
  int tmp = *x;
  *x = *y;
  *y = tmp;
}

int partion(int* arr, int left, int right) {
  int pivot = arr[right]; 
  
  int l = left-1;
  for(int r=left; r<right; r++) {
    if(arr[r] <= pivot) swap(&arr[++l], &arr[r]);
  }
  
  swap(&arr[right], &arr[++l]);
  return l;
}

void qsort(int* arr, int left, int right) {
  if(left >= right) return;

  int pivot = partion(arr, left, right);

  qsort(arr, left, pivot-1);
  qsort(arr, pivot+1, right);
}

/* --------- Multiple threads --------- */
queue_t create_queue() {
  queue_t q;
  q.front = 0;
  q.back = 0;
  q.count = 0;
  
  q.m = PTHREAD_MUTEX_INITIALIZER;
  q.cond_full = PTHREAD_COND_INITIALIZER;
  q.cond_empty = PTHREAD_COND_INITIALIZER;

  return q;
}

void add_task(void (*func)(void*), void* arg) {
  pthread_mutex_lock(&pool.tasks.m);
  
  while(pool.tasks.count == MAX_TASKS)
    pthread_cond_wait(&pool.tasks.cond_full, &pool.tasks.m);

  task_t task = { func, arg };
  pool.tasks.buf[pool.tasks.back] = task;
  pool.tasks.back = (++pool.tasks.back) % MAX_TASKS;

  pool.tasks.count++;

  pthread_cond_signal(&pool.tasks.cond_empty);

  pthread_mutex_unlock(&pool.tasks.m);
}

sort_arg_t* create_sort_arg(int left, int right, int* arr) {
  sort_arg_t* arg = (sort_arg_t*)malloc(sizeof(sort_arg_t));
  arg->left = left;
  arg->right = right;
  arg->arr = arr;

  return arg;
}

void qsort_thread(void* arg) {
  sort_arg_t* sort_arg = (sort_arg_t*)arg;
    
  if(sort_arg->left >= sort_arg->right) return;
  
  /* printf("%d %d\n", sort_arg->left, sort_arg->right); */
  if(sort_arg->right - sort_arg->left <= 1000) {
    qsort(sort_arg->arr, sort_arg->left, sort_arg->right);
    return;
  }
  
  int pivot = partion(sort_arg->arr, sort_arg->left, sort_arg->right);

  sort_arg_t* arg1 = create_sort_arg(sort_arg->left, pivot-1, sort_arg->arr);
  sort_arg_t* arg2 = create_sort_arg(pivot+1, sort_arg->right, sort_arg->arr);

  add_task(qsort_thread, arg1); 
  add_task(qsort_thread, arg2); 

  free(sort_arg);
}


void* worker(void* arg) {
  /* printf("[+] Worker thread\n"); */
  for(;;) {  
    pthread_mutex_lock(&pool.tasks.m);
    
    /* printf("tasks count: %d\n", pool.tasks.count); */
    while(pool.tasks.count == 0 && !pool.is_shutdown) 
      pthread_cond_wait(&pool.tasks.cond_empty, &pool.tasks.m);
    /* printf("tasks count: %d\n", pool.tasks.count); */

    if(pool.tasks.count == 0 && pool.is_shutdown) {
      pthread_mutex_unlock(&pool.tasks.m);
      break;
    }
    pool.is_shutdown = 0;
    
    // Pop task
    task_t task = pool.tasks.buf[pool.tasks.front];
    pool.tasks.front = (++pool.tasks.front) % MAX_TASKS;
    pool.tasks.count--;

    pool.executing_task_counter++;

    pthread_cond_signal(&pool.tasks.cond_full);
    
    /* printf("Executing th count: %d\n", pool.executing_task_counter); */
    pthread_mutex_unlock(&pool.tasks.m);
    
    task.func(task.arg);
    
    pthread_mutex_lock(&pool.tasks.m);
    
    pool.executing_task_counter--;
    /* printf("Executing th count: %d\n", pool.executing_task_counter); */
    if(pool.executing_task_counter == 0) {
      pool.is_shutdown = 1;
      state.end = clock();
      pthread_cond_broadcast(&pool.tasks.cond_empty);
    }

    pthread_mutex_unlock(&pool.tasks.m);
  }

  return NULL;
}

void init_thread_pool(int n, int arr_size, int* arr) {
  pool.ths = (pthread_t*)malloc(sizeof(pthread_t)*n);

  pool.sz = n;
  pool.is_shutdown = 0;
  
  sort_arg_t* arg = create_sort_arg(0, arr_size-1, arr);
  add_task(qsort_thread, arg);
  
  state.start = clock();
  for(int i=0; i<n; i++) {
    pthread_create(&pool.ths[i], NULL, worker, NULL);
  }
}

void desroy_thread_pool() {
  for(int i=0; i<pool.sz; i++) {
    pthread_join(pool.ths[i], NULL);
  }
  
  pthread_mutex_destroy(&pool.tasks.m);
  pthread_cond_destroy(&pool.tasks.cond_full);
  pthread_cond_destroy(&pool.tasks.cond_empty);
  free(pool.ths);
}

int main() {
  FILE* f = fopen("input.txt", "r");
  int n, th_count;

  fscanf(f, "%d", &th_count);
  fscanf(f, "%d", &n);
  
  int* arr = (int*)malloc(sizeof(int)*n);
  for(int i=0; i<n; i++) {
    fscanf(f, "%d", &arr[i]);
  }

  fclose(f);
  
  /* printf("Before Sorting:\n"); */
  /* print_arr(arr, n); */
  /* printf("----------------\n"); */
  
  init_thread_pool(th_count, n, arr);
  
  desroy_thread_pool();  

  /* printf("After Sorting:\n"); */
  /* print_arr(arr, n); */
  /* printf("----------------\n"); */
  
  f = fopen("output.txt", "w");
  fprintf(f, "%d\n%d\n", th_count, n);
  for(int i=0; i<n; i++) {
    fprintf(f, "%d ", arr[i]);
  }
  fclose(f);

  f = fopen("time.txt", "w");
  double t = ((double)(state.end - state.start)) / CLOCKS_PER_SEC * 1000;
  long res = (long)t;
  fprintf(f, "%ld", res);
  fclose(f);

  return 0;
}
