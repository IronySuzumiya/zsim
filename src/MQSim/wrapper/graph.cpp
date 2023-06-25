#include <cassert>
#include "graph.hh"

namespace GraphUtil {

void Graph::read_header_file() {
  INIReader header(graph_path + "/header.ini");
  assert(header.ParseError() == 0);
  
  global_metadata.weighted = header.GetBoolean("metadata", "weighted", false);
  global_metadata.nverts = header.GetInteger("metadata", "nverts", 0);
  global_metadata.ndverts = header.GetInteger("metadata", "ndverts", 0);
  global_metadata.nedges = header.GetInteger("metadata", "nedges", 0);
  global_metadata.nblocks = header.GetInteger("metadata", "nblocks", 0);
  blocks.resize(global_metadata.nblocks);
  global_metadata.ndblocks = header.GetInteger("metadata", "ndblocks", 0);

  bid_t ndblocks = 0;
  dverts.clear();
  for(bid_t bid = 0; bid < global_metadata.nblocks; ++bid) {
    auto&& block = blocks.at(bid);

    block.metadata.vlo        = header.GetInteger("block" + std::to_string(bid), "vlo", 0);
    block.metadata.vup        = header.GetInteger("block" + std::to_string(bid), "vup", 0);
    block.metadata.elo        = header.GetInteger("block" + std::to_string(bid), "elo", 0);
    block.metadata.odg        = header.GetInteger("block" + std::to_string(bid), "odg", 0);
    block.metadata.idg        = header.GetInteger("block" + std::to_string(bid), "idg", 0);
    block.metadata.dense      = header.GetBoolean("block" + std::to_string(bid), "dense", false);

    assert(block.metadata.vlo <= block.metadata.vup);

    if(block.metadata.dense) {
      ++ndblocks;
      assert(block.metadata.vlo == block.metadata.vup);
      auto dvert = dverts.find(block.metadata.vlo);
      if(dvert != dverts.end()) {
        dvert->second.odg += block.metadata.odg;
        dvert->second.idg += block.metadata.idg;
        ++dvert->second.nblocks;
      } else {
        const auto pair = std::make_pair(
          block.metadata.vlo,
          DenseVertex::Metadata {
            .elo = block.metadata.elo,
            .odg = block.metadata.odg,
            .idg = block.metadata.idg,
            .blo = bid,
            .nblocks = 1
          }
        );
        dverts.insert(pair);
      }
      block.metadata.bytes = sizeof(vid_t) * block.metadata.odg;
    } else {
      block.metadata.bytes =
        global_metadata.voffset_size * (block.metadata.vup - block.metadata.vlo + 1)
        + sizeof(vid_t) * block.metadata.odg;
    }
  }

  assert(ndblocks == global_metadata.ndblocks);
  assert(dverts.size() == global_metadata.ndverts);
}

void Graph::import(const std::string& graph_path, uint32_t block_size) {
  this->graph_path = graph_path + "/b" + std::to_string(block_size);
  global_metadata.voffset_size = std::ceil(std::log2(block_size / sizeof(vid_t)) / 8);
  global_metadata.block_size = block_size;
  read_header_file();
}

bid_t Graph::binary_search_block(vid_t vid) const {
  auto it = std::upper_bound(blocks.cbegin(), blocks.cend(), vid,
    [](const vid_t& val, const Block& block) { return val < block.metadata.vup; });
  assert(it != blocks.cend());
  
  return it - blocks.cbegin();
}

};
