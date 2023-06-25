#include <cassert>
#include "ssd_wrapper.hh"

namespace FlashGNN {

namespace Memory {

bool SSDWrapper::check_addr(const FlashAddress& addr) const {
  return addr.channel < get_num_channels() && addr.chip < get_num_chips_per_channel()
    && addr.die < get_num_dies_per_chip() && addr.plane < get_num_planes_per_die()
    && addr.block < get_num_blocks_per_plane() && addr.page < get_num_pages_per_block();
}

void MQSimWrapper::load_ssd_config(const std::string& config_file) {
  std::fstream fs;
	fs.open(config_file, std::ios::in);
  assert(fs);
  std::stringstream ss;
  ss << fs.rdbuf();
  std::string content(ss.str());
  rapidxml::xml_document<> doc;
  char* temp_string = new char[content.length() + 1];
  strcpy(temp_string, content.c_str());
  doc.parse<0>(temp_string);
  rapidxml::xml_node<> *mqsim_config = doc.first_node("Execution_Parameter_Set");
  assert(mqsim_config);
  _exec_params->XML_deserialize(mqsim_config);
  delete[] temp_string;
  fs.close();
}

void MQSimWrapper::load_workload_config(const std::string& workload_file) {
  std::fstream fs;
	fs.open(workload_file, std::ios::in);
  assert(fs);
  std::stringstream ss;
  ss << fs.rdbuf();
  std::string content(ss.str());
  rapidxml::xml_document<> doc;
  char* temp_string = new char[content.length() + 1];
  strcpy(temp_string, content.c_str());
  doc.parse<0>(temp_string);
  rapidxml::xml_node<> *mqsim_io_scenarios = doc.first_node("MQSim_IO_Scenarios");
  assert(mqsim_io_scenarios);
  auto xml_io_scenario = mqsim_io_scenarios->first_node("IO_Scenario");
  auto flow_def = xml_io_scenario->first_node();
  assert(!strcmp(flow_def->name(), "IO_Flow_Parameter_Set_Trace_Based"));
  auto flow = new IO_Flow_Parameter_Set_Trace_Based;
  flow->XML_deserialize(flow_def);
  _exec_params->Host_Configuration.IO_Flow_Definitions.push_back(flow);
  delete []temp_string;
  fs.close();
}

void MQSimWrapper::channel_busy_callback(uint32_t chanid) {
  auto&& chan = _channels.at(chanid);
  assert(!chan.busy);
  chan.busy = true;
  /*if(!chan.reqs.empty()) {
    auto&& req = chan.reqs.front();
    req.already_trans_cycle += _cycle - _last_cycle;
    assert(req.already_trans_cycle <= req.expected_trans_cycle);
  }*/
}

void MQSimWrapper::channel_idle_callback(uint32_t chanid) {
  auto&& chan = _channels.at(chanid);
  assert(chan.busy);
  chan.busy = false;
  /*if(!chan.reqs.empty()) {
    auto&& req = chan.reqs.front();
  }*/
}

/*void MQSimWrapper::handle_req_flash_callback(SSDRequest* req) {
  //req.finished = true;
  req.callback();
  uint32_t chanid = req.addrs.front().channel;
  uint32_t chipid = req.addrs.front().chip;
  uint32_t bytes = req.bytes;
  uint32_t nplanes = req.addrs.size();
  auto&& chip_stats = _channels_epoch_stats.at(chanid).chips_stats.at(chipid);
  switch(req.type) {
    case SSDRequestType::READ:
      ++chip_stats.read_times;
      chip_stats.read_traffic += bytes * nplanes;
      break;
    case SSDRequestType::WRITE:
      ++chip_stats.write_times;
      chip_stats.write_traffic += bytes * nplanes;
      break;
    default:
      assert(false);
  }
}*/

void MQSimWrapper::handle_req_flash(const SSDRequest& req) {
  auto request = new SSD_Components::User_Request;
  request->Stream_id = 0;
  request->Priority_class = IO_Flow_Priority_Class::Priority::HIGH;
  request->STAT_InitiationTime = Simulator->Time();
  switch(req.type) {
    case SSDRequestType::READ_LOCAL:
    case SSDRequestType::READ:
      request->Type = SSD_Components::UserRequestType::READ;
      break;
    case SSDRequestType::WRITE_LOCAL:
    case SSDRequestType::WRITE:
      request->Type = SSD_Components::UserRequestType::WRITE;
      break;
    default:
      assert(false);
  }
  request->Start_LBA = 0; // not used
  request->SizeInSectors = (req.bytes - 1) / SECTOR_SIZE_IN_BYTE + 1;
  request->Size_in_byte = req.bytes;

  uint32_t chanid = req.addrs.front().channel;
  request->channel_busy_callback = std::bind(&MQSimWrapper::channel_busy_callback, this, chanid);
  request->channel_idle_callback = std::bind(&MQSimWrapper::channel_idle_callback, this, chanid);
  request->finish_callback = req.callback;//std::bind(&MQSimWrapper::handle_req_callback, this, req);
  request->local = req.type == SSDRequestType::READ_LOCAL || req.type == SSDRequestType::WRITE_LOCAL;

  for(uint32_t pagecnt = 0; pagecnt < (req.bytes - 1) / get_page_capacity() + 1; ++pagecnt) {
    uint32_t size_in_bytes = std::min(get_page_capacity(), req.bytes - pagecnt * get_page_capacity());
    uint64_t access_status_bitmap = ~(~0x0UL << ((size_in_bytes - 1) / SECTOR_SIZE_IN_BYTE + 1));
    for(auto& addr : req.addrs) {
      SSD_Components::NVM_Transaction_Flash* trans;
      switch(req.type) {
        case SSDRequestType::READ_LOCAL:
        case SSDRequestType::READ:
          trans = new SSD_Components::NVM_Transaction_Flash_RD(
            SSD_Components::Transaction_Source_Type::USERIO, 0,
            size_in_bytes, 0, NO_PPA, // not used
            request, IO_Flow_Priority_Class::Priority::HIGH, 0, access_status_bitmap, CurrentTimeStamp
          );
          break;
        case SSDRequestType::WRITE_LOCAL:
        case SSDRequestType::WRITE:
          trans = new SSD_Components::NVM_Transaction_Flash_WR(
            SSD_Components::Transaction_Source_Type::USERIO, 0,
            size_in_bytes, 0, // not used
            request, IO_Flow_Priority_Class::Priority::HIGH, 0, access_status_bitmap, CurrentTimeStamp
          );
          break;
        default:
          assert(false);
      }


      trans->Address.ChannelID = addr.channel;
      trans->Address.ChipID = addr.chip;
      trans->Address.DieID = addr.die;
      trans->Address.PlaneID = addr.plane;
      trans->Address.BlockID = addr.block + ((addr.page + pagecnt) / get_num_pages_per_block());
      trans->Address.PageID = (addr.page + pagecnt) % get_num_pages_per_block();

      trans->Physical_address_determined = true;

      request->Transaction_list.push_back(trans);
    }
  }

  auto&& tsu = dynamic_cast<SSD_Components::FTL*>(_ssd->Firmware)->TSU;
  if(request->Transaction_list.size() > 0) {
    tsu->Prepare_for_transaction_submit();
    for(const auto& trans : request->Transaction_list) {
      auto&& trans_flash = (SSD_Components::NVM_Transaction_Flash*)trans;
      assert(trans_flash->Physical_address_determined);
      tsu->Submit_transaction(trans_flash);
      if(req.type == SSDRequestType::WRITE_LOCAL || req.type == SSDRequestType::WRITE) {
        auto&& trans_flash_wr = (SSD_Components::NVM_Transaction_Flash_WR*)trans_flash;
        if(trans_flash_wr->RelatedRead != nullptr) {
          tsu->Submit_transaction(trans_flash_wr->RelatedRead);
        }
      }
    }
    tsu->Schedule();
  }
}

void MQSimWrapper::handle_req_channel(const SSDRequest& req) {
  uint32_t chanid = req.addrs.front().channel;
  auto&& chan = _channels.at(chanid);
  TransReq trans_req {
    .bytes = req.bytes,
    .bytes_to_trans = static_cast<float>(req.bytes),
    .board_to_chip = req.type == SSDRequestType::PUSH,
    .callback = req.callback
  };
  chan.reqs.push(trans_req);
  switch (req.type)
  {
    case SSDRequestType::PULL:
      chan.chip_to_board_bytes += req.bytes;
      break;
    case SSDRequestType::PUSH:
      chan.board_to_chip_bytes += req.bytes;
      break;
    default:
      assert(false);
      break;
  }
}

void MQSimWrapper::handle_req(const SSDRequest& req) {
  for(const auto& addr : req.addrs) {
    if(!check_addr(addr)) {
      assert(false);
      return;
    }
  }
  if(req.bytes == 0) {
    req.callback();
  } else {
    switch(req.type) {
      case SSDRequestType::READ_LOCAL:
      case SSDRequestType::READ:
      case SSDRequestType::WRITE_LOCAL:
      case SSDRequestType::WRITE:
        handle_req_flash(req);
        break;
      case SSDRequestType::PULL:
      case SSDRequestType::PUSH:
        handle_req_channel(req);
        break;
      default:
        assert(false);
        break;
    }
  }
}

MQSimWrapper::MQSimWrapper(const GraphUtil::Graph* graph)
  : SSDWrapper(graph), _exec_params(new Execution_Parameter_Set), _ssd(nullptr), _host(nullptr), _sectors_per_page(0),
    _bytes_per_cycle(
      _exec_params->SSD_Device_Configuration.Flash_Channel_Width
      * _exec_params->SSD_Device_Configuration.Channel_Transfer_Rate
      * 1024.0 / 1000.0 * 1024.0 / 1000.0 / 1000.0),
    _output_path(".") {
  Simulator->Reset();

  load_ssd_config("configs/ssd/config_4096_333.xml");
  load_workload_config("configs/ssd/workload_4096_333.xml");
  _ssd = new SSD_Device(&_exec_params->SSD_Device_Configuration, &_exec_params->Host_Configuration.IO_Flow_Definitions);
  _exec_params->Host_Configuration.Input_file_path = "configs/ssd/workload_4096_333";
  _host = new Host_System(&_exec_params->Host_Configuration, _exec_params->SSD_Device_Configuration.Enabled_Preconditioning, _ssd->Host_interface);
  _host->Attach_ssd_device(_ssd);

  _sectors_per_page = _exec_params->SSD_Device_Configuration.Flash_Parameters.Page_Capacity / SECTOR_SIZE_IN_BYTE;
  _channels.resize(_exec_params->SSD_Device_Configuration.Flash_Channel_Count);
  _channels_epoch_stats.resize(_exec_params->SSD_Device_Configuration.Flash_Channel_Count, { {}, 0 });
  for(auto&& channel_epoch_stats : _channels_epoch_stats)
    channel_epoch_stats.chips_stats.resize(_exec_params->SSD_Device_Configuration.Chip_No_Per_Channel, { 0, 0, 0, 0 });
  _channels_stats.resize(_exec_params->SSD_Device_Configuration.Flash_Channel_Count, { {}, 0 });
  for(auto&& channel_stats : _channels_stats)
    channel_stats.chips_stats.resize(_exec_params->SSD_Device_Configuration.Chip_No_Per_Channel, { 0, 0, 0, 0 });

  //assert(get_page_size() * get_num_planes_per_die() == graph->get_global_metadata().subblock_size);

  Simulator->get_ready();
  Simulator->clear_dummy_event();
}

void MQSimWrapper::send_req(const SSDRequest& req) {
  handle_req(req);
}

void MQSimWrapper::tick() {
  _clock_ns += 1.0;
  ++_cycle;

  assert(_cycle > _last_cycle);
  
  Simulator->tick();

  for(uint32_t chanid = 0; chanid < get_num_channels(); ++chanid) {
    auto&& chan = _channels.at(chanid);
    if(chan.busy) continue;
    float bytes_trans = (_cycle - _last_cycle) * _bytes_per_cycle;
    while(!chan.reqs.empty()) {
      auto&& req = chan.reqs.front();
      if(req.bytes_to_trans <= bytes_trans) {
        bytes_trans -= req.bytes_to_trans;
        _channels_epoch_stats.at(chanid).traffic += req.bytes_to_trans;
        if(req.board_to_chip) {
          chan.board_to_chip_bytes -= req.bytes_to_trans;
        } else {
          chan.chip_to_board_bytes -= req.bytes_to_trans;
        }
        req.bytes_to_trans = 0;
        req.callback();
        chan.reqs.pop();
      } else {
        req.bytes_to_trans -= bytes_trans;
        _channels_epoch_stats.at(chanid).traffic += bytes_trans;
        if(req.board_to_chip) {
          chan.board_to_chip_bytes -= bytes_trans;
        } else {
          chan.chip_to_board_bytes -= bytes_trans;
        }
        bytes_trans = 0;
        break;
      }
    }
  }

  _last_cycle = _cycle;
}

bool MQSimWrapper::is_event_tree_empty() const {
  for(auto& chan : _channels) if(!chan.reqs.empty()) return false;
  return Simulator->is_event_tree_empty();
}

uint64_t MQSimWrapper::get_next_event_firetime() const {
  assert(_cycle == _last_cycle);
  uint64_t firetime = Simulator->is_event_tree_empty() ? UINT64_MAX : Simulator->get_next_event_firetime();
  assert(_cycle < firetime);
  for(const auto& chan : _channels) {
    if(!chan.reqs.empty() && !chan.busy) {
      auto&& req = chan.reqs.front();
      float f_latency = req.bytes_to_trans / _bytes_per_cycle;
      assert(f_latency > 0);
      uint64_t latency = std::ceil(f_latency);
      assert(latency > 0);
      uint64_t expected_complete_cycle = _cycle + latency;
      if(expected_complete_cycle < firetime) {
        firetime = expected_complete_cycle;
      }
    }
  }
  assert(_cycle < firetime);
  return firetime;
}

};

};
