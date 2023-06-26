#include <stdint.h>
#include <stdio.h>
#include <vector>
#include <map>
#include <unistd.h>
#include "zsim_hooks.h"

bool edge_list_req_finished = false;
bool node_feature_req_finished = false;

void *heartbeat(void *) {
  while(!edge_list_req_finished || !node_feature_req_finished) {
    zsim_heartbeat();
    usleep(1);
  }
  return NULL;
}

int main(int argc, char* argv[]) {
  pthread_t heartbeat_tid;

  pthread_create(&heartbeat_tid, NULL, heartbeat, NULL);

  zsim_roi_begin();

  DataManagerRequest edge_list_req {
    .type = DataManagerRequestType::EDGE_LIST,
    .val = 233,
    .callback = []() {
      edge_list_req_finished = true;
      printf("node 233 edge list loaded\n");
    }
  };
  send_data_manager_request(&edge_list_req);
  usleep(1);
  DataManagerRequest node_feature_req {
    .type = DataManagerRequestType::NODE_FEATURE,
    .val = 2333,
    .callback = []() {
      node_feature_req_finished = true;
      printf("node 2333 input feature loaded\n");
    }
  };
  send_data_manager_request(&node_feature_req);

  pthread_join(heartbeat_tid, NULL);

  zsim_roi_end();

  return 0;
}
