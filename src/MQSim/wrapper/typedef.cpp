#include "typedef.hh"

namespace FlashGNN {

void node_to_subgraph(Node* node, std::list<Node*>& nodes) {
  for(const auto& child : node->children) {
    node_to_subgraph(child, nodes);
  }
  nodes.push_back(node);
}

void node_to_subgraph_exclude_root_node(Node* node, std::list<Node*>& nodes) {
  for(const auto& child : node->children) {
    node_to_subgraph(child, nodes);
  }
}

uint32_t get_num_nodes_in_subgraph(Node* node, uint32_t depth) {
  uint32_t num_nodes = 1;
  if(node->depth + 1 <= depth) {
    for(const auto& child : node->children) {
      num_nodes += get_num_nodes_in_subgraph(child, depth);
    }
  }
  return num_nodes;
}

void delete_subgraph(Node* node) {
  for(auto& child : node->children) {
    delete_subgraph(child);
  }
  node->children.clear();
  delete node;
}

void delete_subgraph_exclude_root_node(Node* node) {
  for(auto& child : node->children) {
    delete_subgraph(child);
  }
  node->children.clear();
}

bool NodeFeature::operator==(const NodeFeature& other) const {
  if(grad == other.grad && layer == other.layer && vid == other.vid && is_partial == other.is_partial) {
    if(!is_partial) {
      return true;
    } else {
      if(components.size() == other.components.size()) {
        /// FIXME: make sure the number of duplicate components is also the same
        std::unordered_set<NodeFeature, HashNodeFeature> lhs_components_set(components.begin(), components.end());
        std::unordered_set<NodeFeature, HashNodeFeature> rhs_components_set(other.components.begin(), other.components.end());
        if(lhs_components_set.size() == rhs_components_set.size()) {
          for(const auto& node_feature : lhs_components_set) {
            if(rhs_components_set.find(node_feature) == rhs_components_set.end()) {
              return false;
            }
          }
          return true;
        } else {
          return false;
        }
      } else {
        return false;
      }
    }
  } else {
    return false;
  }
}

};