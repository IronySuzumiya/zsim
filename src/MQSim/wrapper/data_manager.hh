#ifndef FLASHGNN_DATA_MANAGER_H
#define FLASHGNN_DATA_MANAGER_H

#include "typedef.hh"
#include "ssd_wrapper.hh"
#include "multifunc_list.hh"
#include "INIReader.h"

namespace FlashGNN {

class DataManager {
public:
  struct PendingReqEntry {
    std::vector<std::function<bool(void)>> hooks;
  };

  struct ReqEntry {
    std::vector<std::function<void(void)>> hooks;
  };

  struct GraphStructureTranslationLayer {
    const Memory::SSDWrapper* ssd;
    const GraphUtil::Graph* graph;

    struct PageReg {
      GraphUtil::bid_t curr_bid;
      GraphUtil::bid_t next_bid;
      uint32_t refs;
    };

    std::vector<PageReg> page_regs;

    struct Stats {
      uint64_t bytes_loaded;
    } stats;

    inline std::vector<Memory::FlashAddress> bid2flashaddr(GraphUtil::bid_t bid) const {
      assert(bid < graph->get_global_metadata().nblocks);
      std::vector<Memory::FlashAddress> addrs(ssd->get_num_planes_per_die());
      uint32_t chipid = bid % (ssd->get_num_chips_per_channel() * ssd->get_num_channels());
      uint32_t nloops = bid / (ssd->get_num_chips_per_channel() * ssd->get_num_channels());
      for(uint32_t planeid = 0; planeid < ssd->get_num_planes_per_die(); ++planeid) {
        Memory::FlashAddress addr {
          .channel = chipid % ssd->get_num_channels(),
          .chip = chipid / ssd->get_num_channels(),
          .die = 0,
          .plane = planeid,
          .block = nloops / ssd->get_num_pages_per_block(),
          .page = nloops % ssd->get_num_pages_per_block()
        };
        assert(ssd->check_addr(addr));
        addrs.at(planeid) = addr;
      }
      return addrs;
    }

    GraphStructureTranslationLayer(const Memory::SSDWrapper* ssd, const GraphUtil::Graph* graph)
      : ssd(ssd), graph(graph), page_regs(ssd->get_num_chips_per_channel() * ssd->get_num_channels(),
        {graph->get_global_metadata().nblocks, graph->get_global_metadata().nblocks, 0}), stats({0}) {}
  };

  struct NodeFeatureTranslationLayer {
    const Memory::SSDWrapper* ssd;
    const GraphUtil::Graph* graph;
    uint32_t node_feature_dim;
    uint32_t node_feature_size;
    uint32_t nodes_per_page;
    uint32_t pages_per_node;
    vgroupid_t nvgroups;

    struct PageReg {
      vgroupid_t curr_vgroupid;
      vgroupid_t next_vgroupid;
      uint32_t refs;
    };

    std::function<std::vector<GraphUtil::vid_t>(vgroupid_t)> vgroupid2vids;
    std::function<vgroupid_t(GraphUtil::vid_t)> vid2vgroupid;

    std::vector<PageReg> page_regs;

    struct InputFeatureStats {
      uint32_t req_entry_hits;
      uint32_t page_reg_hits;
      uint32_t page_reg_misses;

      uint64_t bytes_loaded_from_flash;
      uint64_t bytes_transmitted_via_channel_bus;
    } input_feature_stats;

    inline std::vector<GraphUtil::vid_t> vbucketid2vids(vgroupid_t vbucketid) const {
      assert(vbucketid < nvgroups);
      if(nodes_per_page > 1) {
        std::vector<GraphUtil::vid_t> vids;
        if(vbucketid < nvgroups - 1) {
          vids.resize(nodes_per_page);
        } else {
          assert(vbucketid * nodes_per_page < graph->get_global_metadata().nverts);
          vids.resize(graph->get_global_metadata().nverts - vbucketid * nodes_per_page);
          assert(vids.size() <= nodes_per_page);
        }
        for(uint32_t i = 0; i < vids.size(); ++i) {
          vids.at(i) = vbucketid * nodes_per_page + i;
        }
        return vids;
      } else {
        return { vbucketid };
      }
    }

    inline vgroupid_t vid2vbucketid(GraphUtil::vid_t vid) const {
      assert(vid < graph->get_global_metadata().nverts);
      return nodes_per_page > 1 ? vid / nodes_per_page : vid;
    }

    inline std::vector<Memory::FlashAddress> vgroupid2flashaddr(vgroupid_t vgroupid) const {
      std::vector<Memory::FlashAddress> addrs(ssd->get_num_planes_per_die());
      uint32_t chipid = vgroupid % (ssd->get_num_chips_per_channel() * ssd->get_num_channels());
      uint32_t nloops = vgroupid / (ssd->get_num_chips_per_channel() * ssd->get_num_channels());
      for(uint32_t planeid = 0; planeid < ssd->get_num_planes_per_die(); ++planeid) {
        Memory::FlashAddress addr {
          .channel = chipid % ssd->get_num_channels(),
          .chip = chipid / ssd->get_num_channels(),
          .die = 1,
          .plane = planeid,
          .block = (nloops * pages_per_node) / ssd->get_num_pages_per_block(),
          .page = (nloops * pages_per_node) % ssd->get_num_pages_per_block()
        };
        assert(ssd->check_addr(addr));
        addrs.at(planeid) = addr;
      }
      return addrs;
    }

    inline std::vector<Memory::FlashAddress> vid2flashaddr(GraphUtil::vid_t vid) const {
      return vgroupid2flashaddr(vid2vgroupid(vid));
    }

    NodeFeatureTranslationLayer(const Memory::SSDWrapper* ssd, const GraphUtil::Graph* graph, uint32_t node_feature_dim)
      : ssd(ssd), graph(graph), node_feature_dim(node_feature_dim), node_feature_size(sizeof(uint32_t) * node_feature_dim),
        nodes_per_page(ssd->get_page_capacity() * ssd->get_num_planes_per_die() / node_feature_size),
        pages_per_node((node_feature_size - 1) / (ssd->get_page_capacity() * ssd->get_num_planes_per_die()) + 1),
        nvgroups(nodes_per_page > 1 ? (graph->get_global_metadata().nverts - 1) / nodes_per_page + 1 : graph->get_global_metadata().nverts),
        vgroupid2vids(std::bind(&DataManager::NodeFeatureTranslationLayer::vbucketid2vids, this, std::placeholders::_1)),
        vid2vgroupid(std::bind(&DataManager::NodeFeatureTranslationLayer::vid2vbucketid, this, std::placeholders::_1)),
        page_regs(ssd->get_num_chips_per_channel() * ssd->get_num_channels(), {nvgroups, nvgroups, 0}),
        input_feature_stats({0, 0, 0, 0, 0}) {}
  };

private:
  Memory::SSDWrapper* ssd;
  const GraphUtil::Graph* graph;

  GraphStructureTranslationLayer gstl;
  NodeFeatureTranslationLayer nftl;

  std::vector<MultifuncList<DataChunkTag, PendingReqEntry, HashDataChunkTag>> pending_flash_read_reqs;
  MultifuncList<DataChunkTag, PendingReqEntry, HashDataChunkTag> pending_channel_bus_transmission_reqs;

  std::vector<MultifuncList<DataChunkTag, ReqEntry, HashDataChunkTag>> active_flash_read_reqs;
  MultifuncList<DataChunkTag, ReqEntry, HashDataChunkTag> active_channel_bus_transmission_reqs;

  uint32_t buffer_capacity;
  uint32_t buffer_used;

  std::map<uint64_t, std::function<void(void)>> aggregations;
  std::vector<std::map<uint64_t, std::function<void(void)>>> combinations;
  
  uint32_t aggregator_latency;
  uint32_t pe_latency;
  uint32_t combine_latency;

  std::default_random_engine rand_eng;

  void edge_list_from_flash_to_page_reg_callback(uint32_t chipid, const DataChunkTag& data_chunk_tag);
  bool edge_list_from_flash_to_page_reg(GraphUtil::bid_t bid, std::function<void(void)> callback, bool re_enter = false);
  
  void edge_list_from_page_reg_to_dram_callback_mini(GraphUtil::bid_t bid, uint32_t chipid);
  void edge_list_from_page_reg_to_dram_callback(GraphUtil::bid_t bid, uint32_t chipid, const DataChunkTag& data_chunk_tag);
  bool edge_list_from_page_reg_to_dram(GraphUtil::bid_t bid, std::function<void(void)> callback, bool re_enter = false);

  bool edge_list_from_flash_to_dram(GraphUtil::bid_t bid, std::function<void(void)> callback);

  void node_feature_from_flash_to_page_reg_callback(uint32_t chipid, const DataChunkTag& data_chunk_tag);
  bool node_feature_from_flash_to_page_reg(const NodeFeature& in, std::function<void(void)> callback, bool re_enter = false);

  void node_feature_from_page_reg_to_dram_callback_mini(vgroupid_t vgroupid, uint32_t chipid);
  void node_feature_from_page_reg_to_dram_callback(vgroupid_t vgroupid, uint32_t chipid, const DataChunkTag& data_chunk_tag);
  bool node_feature_from_page_reg_to_dram(const NodeFeature& in, std::function<void(void)> callback, bool re_enter = false);

  bool node_feature_from_flash_to_dram(const NodeFeature& in, std::function<void(void)> callback);

  void show_epoch_gstl_stats();
  void show_epoch_nftl_stats();
  void show_epoch_io_stats();

public:
  DataManager(Memory::SSDWrapper* ssd, const GraphUtil::Graph* graph,
    uint32_t node_feature_dim, uint32_t buffer_capacity,
    uint32_t aggregator_latency, uint32_t pe_latency)
    : ssd(ssd), graph(graph), gstl(ssd, graph), nftl(ssd, graph, node_feature_dim),
      pending_flash_read_reqs(ssd->get_num_chips_per_channel() * ssd->get_num_channels()),
      active_flash_read_reqs(ssd->get_num_chips_per_channel() * ssd->get_num_channels()),
      buffer_capacity(buffer_capacity), buffer_used(0),
      combinations(2), aggregator_latency(aggregator_latency), pe_latency(pe_latency),
      combine_latency(pe_latency * 128 * 2 * ((node_feature_dim - 1) / 128 + 1) * ((node_feature_dim - 1) / 128 + 1)),
      rand_eng(2333) {}
  ~DataManager() {}

  inline uint64_t get_cycle() const {
    return ssd->get_cycle();
  }

  inline void set_cycle(uint64_t cycle) {
    ssd->set_cycle(cycle);
  }

  inline bool pending_reqs_empty() const {
    for(const auto& reqs : pending_flash_read_reqs) {
      if(!reqs.empty()) {
        return false;
      }
    }
    if(!pending_channel_bus_transmission_reqs.empty()) {
      return false;
    }
    return true;
  }

  inline bool active_reqs_empty() const {
    for(const auto& reqs : active_flash_read_reqs) {
      if(!reqs.empty()) {
        return false;
      }
    }
    if(!active_channel_bus_transmission_reqs.empty()) {
      return false;
    }
    return true;
  }

  inline bool busy() const {
    return ssd->busy() || !pending_reqs_empty() || !active_reqs_empty();
  }

  inline vgroupid_t vid_to_vgroupid(GraphUtil::vid_t vid) const {
    return nftl.vid2vgroupid(vid);
  }

  inline uint32_t vid_to_chipid(GraphUtil::vid_t vid) const {
    vgroupid_t vgroupid = nftl.vid2vgroupid(vid);
    
    auto&& addrs = nftl.vgroupid2flashaddr(vgroupid);
    uint32_t chipid = addrs.front().chip * ssd->get_num_channels() + addrs.front().channel;
    assert(chipid == (vgroupid % (ssd->get_num_chips_per_channel() * ssd->get_num_channels())));
    
    return chipid;
  }

  inline bool node_feature_in_page_reg(const NodeFeature& in) const {
    if(!in.is_input_node_feature()) {
      return false;
    }

    vgroupid_t vgroupid = nftl.vid2vgroupid(in.vid);

    auto&& addrs = nftl.vgroupid2flashaddr(vgroupid);
    uint32_t chipid = addrs.front().chip * ssd->get_num_channels() + addrs.front().channel;
    assert(chipid == (vgroupid % (ssd->get_num_chips_per_channel() * ssd->get_num_channels())));
    
    return nftl.page_regs.at(chipid).curr_vgroupid == vgroupid && nftl.page_regs.at(chipid).next_vgroupid == nftl.nvgroups;
  }

  inline uint64_t get_next_event_firetime() const {
    uint64_t firetime = ssd->get_next_event_firetime();
    if(!aggregations.empty() && aggregations.begin()->first < firetime) {
      firetime = aggregations.begin()->first;
    }
    for(auto& combiner : combinations) {
      if(!combiner.empty() && combiner.begin()->first < firetime) {
        firetime = combiner.begin()->first;
      }
    }
    return firetime;
  }

  inline void skip_to_next_event() {
    uint64_t firetime = get_next_event_firetime();
    set_cycle(firetime - 1);
    tick();
  }

  inline void tick() {
    ssd->tick();
    while(!aggregations.empty() && get_cycle() >= aggregations.begin()->first) {
      aggregations.begin()->second();
      aggregations.erase(aggregations.begin());
    }
    for(auto& combiner : combinations) {
      while(!combiner.empty() && get_cycle() >= combiner.begin()->first) {
        combiner.begin()->second();
        combiner.erase(combiner.begin());
      }
    }
  }

  bool load_edge_list_to_dram(GraphUtil::bid_t bid, std::function<void(void)> callback);

  bool load_node_feature_to_dram(const NodeFeature& in, std::function<void(void)> callback);

  inline void aggregate(std::function<void(void)> callback) {
    if(aggregations.empty()) {
      aggregations.insert(std::make_pair(get_cycle() + aggregator_latency, callback));
    } else {
      aggregations.insert(std::make_pair(aggregations.rbegin()->first + aggregator_latency, callback));
    }
  }

  inline void combine(std::function<void(void)> callback) {    
    uint64_t finish_cycle = UINT64_MAX;
    uint32_t combiner_idx = 0;
    for(uint32_t i = 0; i < combinations.size(); ++i) {
      if(combinations.at(i).empty() || combinations.at(i).rbegin()->first < get_cycle() + combine_latency) {
        combiner_idx = i;
        finish_cycle = get_cycle() + combine_latency;
        break;
      } else {
        uint64_t tmp_finish_cycle = combinations.at(i).rbegin()->first + pe_latency;
        if(tmp_finish_cycle < finish_cycle) {
          combiner_idx = i;
          finish_cycle = tmp_finish_cycle;
        }
      }
    }
    combinations.at(combiner_idx).insert(std::make_pair(finish_cycle, callback));
  }

  void flush_pending_flash_read_reqs(uint32_t chipid);
  void flush_pending_channel_bus_transmission_reqs();
};

};

#endif
