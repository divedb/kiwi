#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>

#include <iostream>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int main() {
  std::cout << "Parent locking mutex\n";
  pthread_mutex_lock(&mutex);
  pid_t pid = fork();

  if (pid == 0) {
    // This is child process.
    std::cout << "Child trying to lock mutex...\n";
    // This may deadlock.
    pthread_mutex_lock(&mutex);
    std::cout << "Child acquired mutex\n";
    pthread_mutex_unlock(&mutex);
    _exit(0);
  } else if (pid > 0) {
    sleep(5);
    std::cout << "Parent unlocking mutex\n";
    pthread_mutex_unlock(&mutex);
    wait(NULL);
  } else {
    perror("fork");
  }

  return 0;
}
