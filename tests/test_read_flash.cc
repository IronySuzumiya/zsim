#include <stdint.h>
#include <stdio.h>
#include <vector>
#include <map>
#include <unistd.h>
#include "zsim_hooks.h"

std::map<pthread_t, bool> threads_finished;

void *heartbeat(void *) {
  while(true) {
    bool all_finished = true;
    for(const auto& pair : threads_finished) {
      if(!pair.second) {
        all_finished = false;
        break;
      }
    }
    if(all_finished) {
      printf("all threads finished\n");
      break;
    }
    zsim_heartbeat();
    usleep(1);
  }
  return NULL;
}

void *send_ssd_request(void *) {
  SSDRequest req {
    .type = SSDRequestType::READ,
    .addrs = {
      {0, 0, 0, 0, 0, 4},
      {0, 0, 0, 1, 0, 4},
      {0, 0, 0, 2, 0, 4},
      {0, 0, 0, 3, 0, 4},
    },
    .bytes = 4096,
    .callback = []() { printf("callback\n"); },
  };
  mqsim_read_flash(&req);
  while(!req.finished) {
    usleep(10);
  }
  threads_finished[pthread_self()] = true;
  return NULL;
}

int main(int argc, char* argv[]) {
  pthread_t heartbeat_tid;
  pthread_t send_ssd_request_tid;

  zsim_roi_begin();

  zsim_heartbeat();

  pthread_create(&heartbeat_tid, NULL, heartbeat, NULL);

  pthread_create(&send_ssd_request_tid, NULL, send_ssd_request, NULL);
  threads_finished.insert(std::make_pair(send_ssd_request_tid, false));

  pthread_join(heartbeat_tid, NULL);

  zsim_roi_end();

  return 0;
}
