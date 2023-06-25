#ifndef FLASHGNN_TYPEDEF_H
#define FLASHGNN_TYPEDEF_H

#include "graph.hh"

namespace FlashGNN {

typedef GraphUtil::vid_t vgroupid_t;

struct Node {
  uint32_t depth;
  uint64_t tag;
  GraphUtil::vid_t vid;
  std::unordered_map<Node*, uint32_t> parents;
  std::vector<Node*> children;

  inline bool operator==(const Node& other) const {
    return depth == other.depth && tag == other.tag && vid == other.vid;
  }
};

void node_to_subgraph(Node* node, std::list<Node*>& nodes);
void node_to_subgraph_exclude_root_node(Node* node, std::list<Node*>& nodes);
uint32_t get_num_nodes_in_subgraph(Node* node, uint32_t depth);
void delete_subgraph(Node* node);
void delete_subgraph_exclude_root_node(Node* node);

struct NodeFeature {
  bool grad;
  uint32_t layer;
  uint64_t tag;
  GraphUtil::vid_t vid;
  bool is_partial;
  std::vector<NodeFeature> components;

  inline bool is_input_node_feature() const {
    return !grad && layer == 0 && tag == 0 && !is_partial;
  }

  inline bool is_partial_node_feature() const {
    return !grad && is_partial;
  }

  inline bool is_node_feature_gradient() const {
    return grad;
  }

  bool operator==(const NodeFeature& other) const;
};

struct HashNodeFeature {
  inline std::size_t operator()(const NodeFeature& in) const {
    auto hash1 = std::hash<bool>{}(in.grad);
    auto hash2 = std::hash<uint32_t>{}(in.layer);
    auto hash3 = std::hash<uint64_t>{}(in.tag);
    auto hash4 = std::hash<GraphUtil::vid_t>{}(in.vid);
    auto hash5 = std::hash<bool>{}(in.is_partial);
    auto hash6 = std::hash<size_t>{}(in.components.size());
    return hash1 ^ hash2 ^ hash3 ^ hash4 ^ hash5 ^ hash6;
  }
};

enum class DataChunkType {
  EDGE_LIST,
  NODE_FEATURE,
  NODE_FEATURE_GROUP,
  NUM_TYPES
};

struct DataChunkTag {
  DataChunkType type;
  GraphUtil::bid_t bid;
  NodeFeature node_feature;
  vgroupid_t vgroupid;

  inline bool operator==(const DataChunkTag& other) const {
    return type == other.type && bid == other.bid && node_feature == other.node_feature && vgroupid == other.vgroupid;
  }
};

struct HashDataChunkTag {
  inline std::size_t operator()(const DataChunkTag& chunk) const {
    auto hash1 = std::hash<uint32_t>{}(static_cast<uint32_t>(chunk.type));
    auto hash2 = std::hash<GraphUtil::bid_t>{}(chunk.bid);
    auto hash3 = HashNodeFeature{}(chunk.node_feature);
    auto hash4 = std::hash<vgroupid_t>{}(chunk.vgroupid);
    return hash1 ^ hash2 ^ hash3 ^ hash4;
  }
};

};

#endif
