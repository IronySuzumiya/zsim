#include "data_manager.hh"

namespace FlashGNN {

void DataManager::edge_list_from_flash_to_page_reg_callback(uint32_t chipid, const DataChunkTag& data_chunk_tag) {
  gstl.stats.bytes_loaded += ssd->get_page_capacity() * ssd->get_num_planes_per_die();
  gstl.page_regs.at(chipid).curr_bid = gstl.page_regs.at(chipid).next_bid;
  gstl.page_regs.at(chipid).next_bid = graph->get_global_metadata().nblocks;
  assert(active_flash_read_reqs.at(chipid).hit(data_chunk_tag));
  for(const auto& hook : active_flash_read_reqs.at(chipid).get(data_chunk_tag).hooks) {
    hook();
  }
  active_flash_read_reqs.at(chipid).erase(data_chunk_tag);
}

bool DataManager::edge_list_from_flash_to_page_reg(GraphUtil::bid_t bid, std::function<void(void)> callback, bool re_enter) {
  auto&& addrs = gstl.bid2flashaddr(bid);
  uint32_t chipid = addrs.front().chip * ssd->get_num_channels() + addrs.front().channel;
  assert(chipid == (bid % (ssd->get_num_chips_per_channel() * ssd->get_num_channels())));

  DataChunkTag data_chunk_tag {
    .type = DataChunkType::EDGE_LIST,
    .bid = bid,
    .node_feature = {false, 0, 0, graph->get_global_metadata().nverts},
    .vgroupid = nftl.nvgroups
  };

  if(gstl.page_regs.at(chipid).curr_bid == bid && gstl.page_regs.at(chipid).next_bid == graph->get_global_metadata().nblocks) {
    callback();
    return true;
  } else if(active_flash_read_reqs.at(chipid).hit(data_chunk_tag)) {
    active_flash_read_reqs.at(chipid).get(data_chunk_tag).hooks.push_back(callback);
    return true;
  } else if(gstl.page_regs.at(chipid).refs > 0 || gstl.page_regs.at(chipid).next_bid < graph->get_global_metadata().nblocks) {
    if(!re_enter) {
      auto&& function = std::bind(&DataManager::edge_list_from_flash_to_page_reg, this, bid, callback, true);
      if(pending_flash_read_reqs.at(chipid).hit(data_chunk_tag)) {
        pending_flash_read_reqs.at(chipid).get(data_chunk_tag).hooks.push_back(function);
      } else {
        pending_flash_read_reqs.at(chipid).push_back(data_chunk_tag, {{function}});
      }
    }
    return false;
  } else {
    active_flash_read_reqs.at(chipid).push_back(data_chunk_tag, {{callback}});
    gstl.page_regs.at(chipid).next_bid = bid;
    Memory::SSDRequest req {
      .type = Memory::SSDRequestType::READ_LOCAL,
      .addrs = addrs,
      .bytes = ssd->get_page_capacity(),
      .callback = std::bind(&DataManager::edge_list_from_flash_to_page_reg_callback, this, chipid, data_chunk_tag)
    };
    ssd->send_req(req);
    return true;
  }
}

void DataManager::edge_list_from_page_reg_to_dram_callback_mini(GraphUtil::bid_t bid, uint32_t chipid) {
  assert(gstl.page_regs.at(chipid).curr_bid == bid);
  assert(gstl.page_regs.at(chipid).next_bid == graph->get_global_metadata().nblocks);
  assert(gstl.page_regs.at(chipid).refs > 0);
  --gstl.page_regs.at(chipid).refs;
}

void DataManager::edge_list_from_page_reg_to_dram_callback(GraphUtil::bid_t bid, uint32_t chipid, const DataChunkTag& data_chunk_tag) {
  buffer_used -= graph->get_global_metadata().block_size;
  assert(gstl.page_regs.at(chipid).curr_bid == bid);
  assert(gstl.page_regs.at(chipid).next_bid == graph->get_global_metadata().nblocks);
  assert(gstl.page_regs.at(chipid).refs > 0);
  --gstl.page_regs.at(chipid).refs;
  assert(active_channel_bus_transmission_reqs.hit(data_chunk_tag));
  for(const auto& hook : active_channel_bus_transmission_reqs.get(data_chunk_tag).hooks) {
    hook();
  }
  active_channel_bus_transmission_reqs.erase(data_chunk_tag);

  flush_pending_flash_read_reqs(chipid);
}

bool DataManager::edge_list_from_page_reg_to_dram(GraphUtil::bid_t bid, std::function<void(void)> callback, bool re_enter) {
  auto&& addrs = gstl.bid2flashaddr(bid);
  uint32_t chipid = addrs.front().chip * ssd->get_num_channels() + addrs.front().channel;
  assert(chipid == (bid % (ssd->get_num_chips_per_channel() * ssd->get_num_channels())));
  
  assert(gstl.page_regs.at(chipid).curr_bid == bid);
  assert(gstl.page_regs.at(chipid).next_bid == graph->get_global_metadata().nblocks);
  if(!re_enter) {
    ++gstl.page_regs.at(chipid).refs;
  }

  DataChunkTag data_chunk_tag {
    .type = DataChunkType::EDGE_LIST,
    .bid = bid,
    .node_feature = {false, 0, 0, graph->get_global_metadata().nverts},
    .vgroupid = nftl.nvgroups
  };

  if(active_channel_bus_transmission_reqs.hit(data_chunk_tag)) {
    active_channel_bus_transmission_reqs.get(data_chunk_tag).hooks.push_back(
      std::bind(&DataManager::edge_list_from_page_reg_to_dram_callback_mini, this, bid, chipid)
    );
    active_channel_bus_transmission_reqs.get(data_chunk_tag).hooks.push_back(callback);
    return true;
  } else if(buffer_used + graph->get_global_metadata().block_size > buffer_capacity) {
    if(!re_enter) {
      auto&& function = std::bind(&DataManager::edge_list_from_page_reg_to_dram, this, bid, callback, true);
      if(pending_channel_bus_transmission_reqs.hit(data_chunk_tag)) {
        pending_channel_bus_transmission_reqs.get(data_chunk_tag).hooks.push_back(function);
      } else {
        pending_channel_bus_transmission_reqs.push_back(data_chunk_tag, {{function}});
      }
    }
    return false;
  } else {
    active_channel_bus_transmission_reqs.push_back(data_chunk_tag, {{callback}});
    buffer_used += graph->get_global_metadata().block_size;
    Memory::SSDRequest req {
      .type = Memory::SSDRequestType::PULL,
      .addrs = addrs,
      .bytes = graph->get_global_metadata().block_size,
      .callback = std::bind(&DataManager::edge_list_from_page_reg_to_dram_callback, this, bid, chipid, data_chunk_tag)
    };
    ssd->send_req(req);
    return true;
  }
}

bool DataManager::edge_list_from_flash_to_dram(GraphUtil::bid_t bid, std::function<void(void)> callback) {
  return edge_list_from_flash_to_page_reg(bid, std::bind(&DataManager::edge_list_from_page_reg_to_dram, this, bid, callback, false));
}

bool DataManager::load_edge_list_to_dram(GraphUtil::vid_t vid, std::function<void(void)> callback) {
  if(graph->is_dvert(vid)) {
    auto&& dm = graph->get_dvert_metadata(vid);
    for(GraphUtil::bid_t bid = dm.blo; bid < dm.blo + dm.nblocks - 1; ++bid) {
      edge_list_from_flash_to_dram(bid, [](){});
    }
    return edge_list_from_flash_to_dram(dm.blo + dm.nblocks - 1, callback);
  } else {
    GraphUtil::bid_t bid = graph->binary_search_block(vid);
    assert(bid < graph->get_global_metadata().nblocks);
    return edge_list_from_flash_to_dram(bid, callback);
  }
}

void DataManager::node_feature_from_flash_to_page_reg_callback(uint32_t chipid, const DataChunkTag& data_chunk_tag) {
  nftl.input_feature_stats.bytes_loaded_from_flash += ssd->get_page_capacity() * ssd->get_num_planes_per_die();
  nftl.page_regs.at(chipid).curr_vgroupid = nftl.page_regs.at(chipid).next_vgroupid;
  nftl.page_regs.at(chipid).next_vgroupid = nftl.nvgroups;
  assert(active_flash_read_reqs.at(chipid).hit(data_chunk_tag));
  for(const auto& hook : active_flash_read_reqs.at(chipid).get(data_chunk_tag).hooks) {
    hook();
  }
  active_flash_read_reqs.at(chipid).erase(data_chunk_tag);
}

bool DataManager::node_feature_from_flash_to_page_reg(const NodeFeature& in, std::function<void(void)> callback, bool re_enter) {
  assert(in.is_input_node_feature());

  vgroupid_t vgroupid = nftl.vid2vgroupid(in.vid);

  auto&& addrs = nftl.vgroupid2flashaddr(vgroupid);
  uint32_t chipid = addrs.front().chip * ssd->get_num_channels() + addrs.front().channel;
  assert(chipid == (vgroupid % (ssd->get_num_chips_per_channel() * ssd->get_num_channels())));

  DataChunkTag data_chunk_tag {
    .type = DataChunkType::NODE_FEATURE_GROUP,
    .bid = graph->get_global_metadata().nblocks,
    .node_feature = {false, 0, 0, graph->get_global_metadata().nverts},
    .vgroupid = vgroupid
  };

  if(nftl.page_regs.at(chipid).curr_vgroupid == vgroupid
    && nftl.page_regs.at(chipid).next_vgroupid == nftl.nvgroups) {
    callback();
    return true;
  } else if(active_flash_read_reqs.at(chipid).hit(data_chunk_tag)) {
    ++nftl.input_feature_stats.req_entry_hits;
    active_flash_read_reqs.at(chipid).get(data_chunk_tag).hooks.push_back(callback);
    return true;
  } else if(nftl.page_regs.at(chipid).refs > 0 || nftl.page_regs.at(chipid).next_vgroupid < nftl.nvgroups) {
    if(!re_enter) {
      auto&& function = std::bind(&DataManager::node_feature_from_flash_to_page_reg, this, in, callback, true);
      if(pending_flash_read_reqs.at(chipid).hit(data_chunk_tag)) {
        pending_flash_read_reqs.at(chipid).get(data_chunk_tag).hooks.push_back(function);
      } else {
        pending_flash_read_reqs.at(chipid).push_back(data_chunk_tag, {{function}});
      }
    }
    return false;
  } else {
    active_flash_read_reqs.at(chipid).push_back(data_chunk_tag, {{callback}});
    nftl.page_regs.at(chipid).next_vgroupid = vgroupid;
    Memory::SSDRequest req {
      .type = Memory::SSDRequestType::READ_LOCAL,
      .addrs = addrs,
      .bytes = ssd->get_page_capacity(),
      .callback = std::bind(&DataManager::node_feature_from_flash_to_page_reg_callback, this, chipid, data_chunk_tag)
    };
    ssd->send_req(req);
    return true;
  }
}

void DataManager::node_feature_from_page_reg_to_dram_callback_mini(vgroupid_t vgroupid, uint32_t chipid) {
  assert(nftl.page_regs.at(chipid).curr_vgroupid == vgroupid);
  assert(nftl.page_regs.at(chipid).next_vgroupid == nftl.nvgroups);
  assert(nftl.page_regs.at(chipid).refs > 0);
  --nftl.page_regs.at(chipid).refs;
}

void DataManager::node_feature_from_page_reg_to_dram_callback(vgroupid_t vgroupid, uint32_t chipid, const DataChunkTag& data_chunk_tag) {
  buffer_used -= nftl.node_feature_size;
  nftl.input_feature_stats.bytes_transmitted_via_channel_bus += nftl.node_feature_size;
  assert(nftl.page_regs.at(chipid).curr_vgroupid == vgroupid);
  assert(nftl.page_regs.at(chipid).next_vgroupid == nftl.nvgroups);
  assert(nftl.page_regs.at(chipid).refs > 0);
  --nftl.page_regs.at(chipid).refs;
  assert(active_channel_bus_transmission_reqs.hit(data_chunk_tag));
  for(const auto& hook : active_channel_bus_transmission_reqs.get(data_chunk_tag).hooks) {
    hook();
  }
  active_channel_bus_transmission_reqs.erase(data_chunk_tag);

  flush_pending_flash_read_reqs(chipid);
}

bool DataManager::node_feature_from_page_reg_to_dram(const NodeFeature& in, std::function<void(void)> callback, bool re_enter) {
  assert(in.is_input_node_feature());

  vgroupid_t vgroupid = nftl.vid2vgroupid(in.vid);

  auto&& addrs = nftl.vgroupid2flashaddr(vgroupid);
  uint32_t chipid = addrs.front().chip * ssd->get_num_channels() + addrs.front().channel;
  assert(chipid == (vgroupid % (ssd->get_num_chips_per_channel() * ssd->get_num_channels())));

  DataChunkTag data_chunk_tag {
    .type = DataChunkType::NODE_FEATURE,
    .bid = graph->get_global_metadata().nblocks,
    .node_feature = in,
    .vgroupid = vgroupid
  };
  
  assert(nftl.page_regs.at(chipid).curr_vgroupid == vgroupid);
  assert(nftl.page_regs.at(chipid).next_vgroupid == nftl.nvgroups);
  if(!re_enter) {
    ++nftl.page_regs.at(chipid).refs;
  }

  if(active_channel_bus_transmission_reqs.hit(data_chunk_tag)) {
    ++nftl.input_feature_stats.req_entry_hits;
    active_channel_bus_transmission_reqs.get(data_chunk_tag).hooks.push_back(
      std::bind(&DataManager::node_feature_from_page_reg_to_dram_callback_mini, this, vgroupid, chipid)
    );
    active_channel_bus_transmission_reqs.get(data_chunk_tag).hooks.push_back(callback);
    return true;
  } else if(buffer_used + nftl.node_feature_size > buffer_capacity) {
    if(!re_enter) {
      auto&& function = std::bind(&DataManager::node_feature_from_page_reg_to_dram, this, in, callback, true);
      if(pending_channel_bus_transmission_reqs.hit(data_chunk_tag)) {
        pending_channel_bus_transmission_reqs.get(data_chunk_tag).hooks.push_back(function);
      } else {
        pending_channel_bus_transmission_reqs.push_back(data_chunk_tag, {{function}});
      }
    }
    return false;
  } else {
    active_channel_bus_transmission_reqs.push_back(data_chunk_tag, {{callback}});
    buffer_used += nftl.node_feature_size;
    Memory::SSDRequest req {
      .type = Memory::SSDRequestType::PULL,
      .addrs = addrs,
      .bytes = nftl.node_feature_size,
      .callback = std::bind(&DataManager::node_feature_from_page_reg_to_dram_callback, this, vgroupid, chipid, data_chunk_tag)
    };
    ssd->send_req(req);
    return true;
  }
}

bool DataManager::node_feature_from_flash_to_dram(const NodeFeature& in, std::function<void(void)> callback) {
  assert(in.is_input_node_feature());
  return node_feature_from_flash_to_page_reg(in, std::bind(&DataManager::node_feature_from_page_reg_to_dram, this, in, callback, false));
}

bool DataManager::load_node_feature_to_dram(const NodeFeature& in, std::function<void(void)> callback) {
  assert(in.is_input_node_feature());
  return node_feature_from_flash_to_dram(in, callback);
}

void DataManager::flush_pending_flash_read_reqs(uint32_t chipid) {
  while(!pending_flash_read_reqs.at(chipid).empty()) {
    auto&& hooks = pending_flash_read_reqs.at(chipid).front()->entry.hooks;
    uint32_t cnt = 0;
    for(const auto& hook : hooks) {
      if(hook()) {
        ++cnt;
      } else {
        break;
      }
    }
    assert(cnt == 0 || cnt == hooks.size());
    if(cnt == 0) {
      break;
    } else {
      pending_flash_read_reqs.at(chipid).pop_front();
    }
  }
}

void DataManager::flush_pending_channel_bus_transmission_reqs() {
  while(!pending_channel_bus_transmission_reqs.empty()) {
    auto&& hooks = pending_channel_bus_transmission_reqs.front()->entry.hooks;
    uint32_t cnt = 0;
    for(const auto& hook : hooks) {
      if(hook()) {
        ++cnt;
      } else {
        break;
      }
    }
    assert(cnt == 0 || cnt == hooks.size());
    if(cnt == 0) {
      break;
    } else {
      pending_channel_bus_transmission_reqs.pop_front();
    }
  }
}

};
