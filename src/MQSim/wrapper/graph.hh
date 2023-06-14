#ifndef GRAPH_UTIL_GRAPH_H
#define GRAPH_UTIL_GRAPH_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <cmath>
#include <vector>
#include <string>
#include <unordered_map>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef __linux__
#include <sys/sysinfo.h>
#endif

#include "INIReader.h"

namespace GraphUtil {

#ifdef VID64BIT
typedef uint64_t vid_t;
#else
typedef uint32_t vid_t;
#endif
typedef uint64_t eid_t;
typedef uint32_t beid_t;
typedef float eprop_t;
typedef uint32_t bid_t;

class Graph {
public:
  struct Edge {
    vid_t dst;
  };
  
  struct Vertex {
    std::vector<Edge> edges;
  };

  struct GlobalMetadata {
    bool weighted;
    vid_t nverts;
    vid_t ndverts;
    eid_t nedges;
    bid_t nblocks;
    bid_t ndblocks;

    uint32_t block_size;

    uint32_t voffset_size;
  };

  struct Block {
    struct Metadata {
      vid_t vlo;
      vid_t vup;
      eid_t elo;
      uint32_t odg;
      eid_t idg;
      bool dense;
      uint32_t bytes;
    } metadata;
  };

  struct DenseVertex {
    struct Metadata {
      eid_t elo;
      beid_t odg;
      eid_t idg;
      
      bid_t blo;
      bid_t nblocks;
    } metadata;
  };

protected:
  std::string graph_path;
  GlobalMetadata global_metadata;
  std::vector<Block> blocks;
  std::unordered_map<vid_t, DenseVertex::Metadata> dverts;
  std::vector<Graph::Vertex> vertices;

  void read_header_file();

public:
  Graph() {}
  virtual ~Graph() { blocks.clear(); }
  
  void import(const std::string& graph_path, uint32_t block_size);

  const GlobalMetadata& get_global_metadata () const { return global_metadata; }
  const Block::Metadata& get_block_metadata(bid_t bid) const { return blocks.at(bid).metadata; }
  const DenseVertex::Metadata& get_dvert_metadata(vid_t vid) const { return dverts.find(vid)->second; }

  bool is_dvert(vid_t vid) const { return dverts.find(vid) != dverts.end(); }
  bool is_vert_in_block(vid_t vid, bid_t bid) const { return vid >= blocks.at(bid).metadata.vlo && vid < blocks.at(bid).metadata.vup; }
  bid_t binary_search_block(vid_t vid) const;

  eid_t get_vert_odg(vid_t vid) const { return vertices.at(vid).edges.size(); }
  vid_t get_vert_edge_dst(vid_t vid, eid_t eid) const { return vertices.at(vid).edges.at(eid).dst; }
};

};

#endif
