// cpennello 2015-09-10

#include "stdio.h"
#include "stdlib.h"

#include "apr_shm.h"
#include "apr_errno.h"
#include "apr_general.h"

#include "httpd.h"
#include "scoreboard.h"

static const char *defaultfile = "/var/run/apache_runtime_status";

static const char *scoreboard_key[] = {
  "noproc",
  "startingup",
  "waitingforconn",
  "readingrequest",
  "sendingreply",
  "keepaliveread",
  "logging",
  "dnslookup",
  "closingconnection",
  "gracefullyfinishing",
  "idleworkercleanup",
};

static const char *busyidle_key[] = {
  "busy",
  "idle",
};

int dump(const char *filename) {
  apr_status_t ret;
  apr_pool_t *pool;
  apr_shm_t *m;
  apr_size_t size, size_expect;
  void *ptr;
  global_score *global;
  process_score *parent;
  worker_score *servers;

  // Lots of APR SHM initialization.

  if ((ret = apr_initialize()) != APR_SUCCESS) {
    fprintf(stderr, "apr_initialize failed: %d\n", ret);
    return 3;
  }

  if ((ret = apr_pool_create(&pool, NULL)) != APR_SUCCESS) {
    fprintf(stderr, "apr_pool_create failed: %d\n", ret);
    return 4;
  }

  if ((ret = apr_shm_attach(&m, filename, pool)) != APR_SUCCESS) {
    fprintf(stderr, "apr_shm_attach failed: %d\n", ret);
    return 4;
  }

  if (!(ptr = apr_shm_baseaddr_get(m))) {
    fprintf(stderr, "apr_shm_baseaddr_get failed\n");
    return 4;
  }

  // We'll use the global structure in doing further size checking, so
  // we check that independently first.
  if ((size = apr_shm_size_get(m)) < sizeof(*global)) {
    fprintf(stderr, "shm pointer too small (expected %lu, got %lu)\n",
      sizeof(*global), size);
    return 4;
  }

  global = ptr;
  ptr += sizeof(*global);

  // Expected size of the whole structure.
  size_expect =
      sizeof(*global )
    + sizeof(*parent ) * global->server_limit
    + sizeof(*servers) * global->server_limit
                       * global->thread_limit
#ifdef SCOREBOARD_BALANCERS
    + sizeof(lb_score) * global->lb_limit
#endif
    ;
  if (size != size_expect) {
    fprintf(stderr, "shm size incorrect\n");
    return 4;
  }

  parent = ptr;
  ptr += sizeof(*parent) * global->server_limit;
  servers = ptr;

  // Counts for each of the status codes from scoreboard.h: 0-10.
  int statushist[SERVER_NUM_STATUS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  // Counts for busy/idle summary statistic.
  int busyidlehist[2] = {0, 0};

  int i, j;
  for (i = 0; i < global->server_limit; ++i) {
    worker_score *sub_servers = servers + i;
    for (j = 0; j < global->thread_limit; ++j) {
      worker_score *server = sub_servers + j;
      unsigned char status = server->status;
      // Track status count.
      ++statushist[status];
      // Track busy/idle summary statistic.
      // Dead servers are neither busy nor idle.
      if (status == SERVER_DEAD) continue;
      ++busyidlehist[status == SERVER_READY];
    }
  }

  // Dump specific status statistics.
  for (i = 0; i < SERVER_NUM_STATUS; ++i) {
    printf("worker.%s %d\n", scoreboard_key[i], statushist[i]);
  }

  // Dump busy/idle summary statistics.
  for (i = 0; i < 2; ++i) {
    printf("worker.%s %d\n", busyidle_key[i], busyidlehist[i]);
  }

  return 0;
}

void usage(char **argv) {
  fprintf(stderr, "usage: %s [-h] [/path/to/sbfile]\n", argv[0]);
  fprintf(stderr, "  dumps worker statistics from apache scoreboard\n");
  fprintf(stderr, "  sbfile path defaults to \"%s\"\n", defaultfile);
  exit(2);
}

int main(int argc, char **argv) {
  // Read score board file name from first command-line argument, if
  // available, otherwise, use default.
  const char *filename;
  if (argc == 1) {
    filename = defaultfile;
  } else if (argc == 2) {
    filename = argv[1];
    size_t filename_len = strlen(filename);
    if (0 == strncmp(filename, "-h", filename_len < 2 ? filename_len : 2)) {
      usage(argv);
    }
  } else {
    usage(argv);
  }

  int ret;
  if ((ret = dump(filename)) != 3) {
    apr_terminate();
  }
  return ret;
}
