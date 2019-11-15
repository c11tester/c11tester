#include "common.h"
#include <unistd.h>
#include "model.h"
#include <dlfcn.h>

static int (*pipe_init_p)(int filep[2]) = NULL;

int pipe(int fildes[2]) {
  if (!model) {
    snapshot_system_init(10000, 1024, 1024, 40000);
    model = new ModelChecker();
    model->startChecker();
  }
  if (!pipe_init_p) {
    pipe_init_p = (int (*)(int file[2])) dlsym(RTLD_NEXT, "pipe");
    if ((error = dlerror()) != NULL) {
      fputs(error, stderr);
      exit(EXIT_FAILURE);
    }
  }
  pipe_init_p(filedes);
}
