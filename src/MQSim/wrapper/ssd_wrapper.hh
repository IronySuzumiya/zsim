#ifndef SSD_WRAPPER_H
#define SSD_WRAPPER_H

#include <random>
#include <unistd.h>

#include "module.hh"
#include "graph.hh"

#include "../host/Host_IO_Request.h"
#include "../ssd/SSD_Defs.h"
#include "../exec/Execution_Parameter_Set.h"
#include "../exec/SSD_Device.h"
#include "../exec/Host_System.h"
#include "../utils/rapidxml/rapidxml.hpp"
#include "../utils/DistributionTypes.h"
#include "../sim/Sim_Defs.h"
#include "../sim/EventTree.h"
#include "../sim/Sim_Object.h"
#include "../sim/Sim_Event.h"

namespace FlashGNN {

namespace Memory {

struct FlashAddress {
  uint32_t channel;
  uint32_t chip;
  uint32_t die;
  uint32_t plane;
  uint32_t block;
  uint32_t page;

  template<typename OStream>
  friend OStream &operator<<(OStream &os, const FlashAddress& addr) {
    return os << addr.channel << ", " << addr.chip << ", " << addr.die << ", " << addr.plane << ", " << addr.block << ", " << addr.page;
  }
};

enum class SSDRequestType {
  READ_LOCAL,
  READ,
  WRITE_LOCAL,
  WRITE,
  PULL,
  PUSH,
  NUM_TYPE
};

struct SSDRequest {
  SSDRequestType type;
  std::vector<FlashAddress> addrs;
  uint32_t bytes;
  std::function<void(void)> callback;
};

class SSDWrapper : public Module {
protected:
  const GraphUtil::Graph* _graph;

public:
  SSDWrapper(const GraphUtil::Graph* graph) : Module(1.0), _graph(graph) {}
  virtual ~SSDWrapper() {}

  bool check_addr(const FlashAddress& addr) const;

  virtual void send_req(const SSDRequest& req) = 0;

  virtual uint32_t get_num_channels() const = 0;
  virtual uint32_t get_num_chips_per_channel() const = 0;
  virtual uint32_t get_num_dies_per_chip() const = 0;
  virtual uint32_t get_num_planes_per_die() const = 0;
  virtual uint32_t get_num_blocks_per_plane() const = 0;
  virtual uint32_t get_num_pages_per_block() const = 0;
  virtual uint32_t get_page_capacity() const = 0;

  virtual void skip_to_next_event() = 0;
  virtual bool is_event_tree_empty() const = 0;
  virtual uint64_t get_next_event_firetime() const = 0;
};

class MQSimWrapper : public SSDWrapper {
private:
  Execution_Parameter_Set* _exec_params;
  SSD_Device* _ssd;
  Host_System* _host;
  uint32_t _sectors_per_page;
  //uint32_t _channel_buffer_size;
  float _bytes_per_cycle;

  struct TransReq {
    uint32_t bytes;
    float bytes_to_trans;
    bool board_to_chip;
    std::function<void(void)> callback;
  };

  struct FlashChannel {
    std::queue<TransReq> reqs;
    float board_to_chip_bytes;
    float chip_to_board_bytes;
    bool busy;
  };

  struct FlashChipStats {
    uint64_t read_times;
    uint64_t read_traffic;
    uint64_t write_times;
    uint64_t write_traffic;
  };

  struct FlashChannelStats {
    std::vector<FlashChipStats> chips_stats;
    float traffic;
  };

  std::vector<FlashChannel> _channels;
  std::vector<FlashChannelStats> _channels_epoch_stats;
  std::vector<FlashChannelStats> _channels_stats;

  std::string _output_path;

  void load_ssd_config(const std::string& config_file);
  void load_workload_config(const std::string& workload_file);

  void channel_busy_callback(uint32_t chanid);
  void channel_idle_callback(uint32_t chanid);

  //void handle_req_flash_callback(SSDRequest* req);
  void handle_req_flash(const SSDRequest& req);
  void handle_req_channel(const SSDRequest& req);
  void handle_req(const SSDRequest& req);

public:
  MQSimWrapper(const GraphUtil::Graph* graph);
  ~MQSimWrapper() { /*delete _exec_params;*/ delete _ssd; delete _host; }

  void send_req(const SSDRequest& req) override;

  void tick() override;

  uint32_t get_num_channels() const override { return _exec_params->SSD_Device_Configuration.Flash_Channel_Count; }
  uint32_t get_num_chips_per_channel() const override { return _exec_params->SSD_Device_Configuration.Chip_No_Per_Channel; }
  uint32_t get_num_dies_per_chip() const override { return _exec_params->SSD_Device_Configuration.Flash_Parameters.Die_No_Per_Chip; }
  uint32_t get_num_planes_per_die() const override { return _exec_params->SSD_Device_Configuration.Flash_Parameters.Plane_No_Per_Die; }
  uint32_t get_num_blocks_per_plane() const override { return _exec_params->SSD_Device_Configuration.Flash_Parameters.Block_No_Per_Plane; }
  uint32_t get_num_pages_per_block() const override { return _exec_params->SSD_Device_Configuration.Flash_Parameters.Page_No_Per_Block; }
  uint32_t get_page_capacity() const override { return _exec_params->SSD_Device_Configuration.Flash_Parameters.Page_Capacity; }

  bool busy() const override { return !is_event_tree_empty(); }

  void skip_to_next_event() override { uint64_t firetime = get_next_event_firetime(); set_cycle(firetime - 1); tick(); }
  bool is_event_tree_empty() const override;
  uint64_t get_next_event_firetime() const override;
  
  uint64_t get_cycle() const override { assert(_cycle == Simulator->Time()); return _cycle; }
  float get_clock_ns() const override { assert(static_cast<uint64_t>(_clock_ns) == Simulator->Time()); return _clock_ns; }

  void set_cycle(uint64_t cycle) override { _cycle = cycle; _clock_ns = cycle; Simulator->set_sim_time(cycle); }
};

};

};

#endif
