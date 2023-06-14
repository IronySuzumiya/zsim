#ifndef FLASHGNN_OPTIONS_H
#define FLASHGNN_OPTIONS_H

#include <iostream>
#include <string>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/program_options.hpp>

#include "fmt/format.h"

namespace FlashGNN {

struct Options {
  // Log Options
  struct LogOptions {
    std::string level;
    //std::string path;
  } log_options;

  // Graph Options
  struct GraphOptions {
    std::string path;
    uint32_t block_size;
  } graph_options;

  // SSD Options
  struct SSDOptions {
    std::string dir;
    std::string config;
    std::string config_path;
    std::string workload_path;
    uint64_t buffer_capacity;
    bool discard_re_data;
  } ssd_options;

  // DRAM Options
  struct DRAMOptions {
    bool use_ideal_memory;
    bool sure_to_hit;
    uint64_t capacity;
    float bandwidth;
    bool split_space;
    uint64_t edge_lists_capacity;
    uint64_t node_features_capacity;
  } dram_options;

  // GNN Options
  struct GNNOptions {
    std::string model;
    bool train;
    std::string query_gen_mode;
    std::string query_file;
    uint32_t num_queries;
    uint32_t num_layers;
    std::string s_samples_per_layer;
    std::vector<uint32_t> samples_per_layer; // internal variable
    uint32_t node_feature_dim;
    uint32_t num_nodes_per_subgraph; // internal variable
  } gnn_options;

  // FlashGNN Options
  struct FlashGNNOptions {
    uint32_t seed;
    std::string query_gen_path;
    std::string batch_forming_mode;
    uint32_t lock_root_threshold;
    std::string query_overlap_level;
    std::string dataflow_scheduling;
    bool reuse_sampling_results;
    uint32_t batch_size;
    uint64_t cache_capacity;
    uint64_t aggregation_latency;
    uint32_t num_combiners;
    uint64_t mac_op_latency;
    uint32_t num_sampler_task_entries;
    uint32_t num_processor_task_entries;
    uint32_t sampling_cache_size;
    uint32_t ready_cache_size;
    bool separate_islands;
    struct SubPEOptions {
      bool enabled;
      uint64_t buffer_capacity;
      float bandwidth;
      uint64_t aggregation_latency;
      uint32_t num_task_entries;
    } subpe_options;
  } flashgnn_options;

  bool parse(long long int argc, char** argv) {
    namespace po = boost::program_options;

    po::options_description log("Log Options");
    log.add_options()
      ("help", "Display help message")
      ("log.level", po::value<std::string>(&log_options.level)->default_value("error"),
        "spdlog level (trace, debug, info, warn, error, critical, off)")
      //("log.path", po::value<std::string>(&log_options.path)->default_value("./"),
      //  "path to output statistic information")
    ;

    po::options_description graph("Graph Options");
    graph.add_options()
      ("graph.path", po::value<std::string>(&graph_options.path)->required(),
        "path to flashwalker format graph dataset")
      ("graph.block_size", po::value<uint32_t>(&graph_options.block_size)->default_value(65536),
        "max swapped-out blocks to buffer in memory (simulator)")
    ;

    po::options_description ssd("SSD Options");
    ssd.add_options()
      ("ssd.dir", po::value<std::string>(&ssd_options.dir)->default_value("configs/ssd"),
        "directory containing MQSim config & workload files")
      ("ssd.config", po::value<std::string>(&ssd_options.config)->default_value("16384_333"),
        "path to MQSim config & workload files is set to [dir]/config_[config].xml and [dir]/workload_[config].xml")
      ("ssd.config_path", po::value<std::string>(&ssd_options.config_path)->default_value(""),
        "path to MQSim config file, overwrite [config] (postfix should be .xml)")
      ("ssd.workload_path", po::value<std::string>(&ssd_options.workload_path)->default_value(""),
        "path to MQSim workload file, overwrite [config] (postfix should be .xml)")
      ("ssd.buffer_capacity", po::value<uint64_t>(&ssd_options.buffer_capacity)->default_value(16777216),
        "capacity of channel transmission buffer in bytes")
      ("ssd.discard_re_data", po::bool_switch(&ssd_options.discard_re_data)->default_value(false),
        "discard redundant data in page register and only transmit requested data")
    ;

    po::options_description dram("DRAM Options");
    dram.add_options()
      ("dram.use_ideal_memory", po::bool_switch(&dram_options.use_ideal_memory)->default_value(false),
        "use ideal memory (infinite capacity)")
      ("dram.sure_to_hit", po::bool_switch(&dram_options.sure_to_hit)->default_value(false),
        "all input node features are in DRAM")
      ("dram.capacity", po::value<uint64_t>(&dram_options.capacity)->default_value(8192),
        "DRAM capacity in MB")
      ("dram.bandwidth", po::value<float>(&dram_options.bandwidth)->default_value(64),
        "DRAM bandwidth in GB/s")
      ("dram.split_space", po::bool_switch(&dram_options.split_space)->default_value(false),
        "split DRAM space into edge lists' and node features'")
      ("dram.edge_lists_capacity", po::value<uint64_t>(&dram_options.edge_lists_capacity)->default_value(1024),
        "DRAM capacity for edge lists in MB (only used when split_space is true)")
      ("dram.node_features_capacity", po::value<uint64_t>(&dram_options.node_features_capacity)->default_value(7168),
        "DRAM capacity for node features in MB (only used when split_space is true)")
    ;

    po::options_description gnn("GNN Options");
    gnn.add_options()
      ("gnn.model", po::value<std::string>(&gnn_options.model)->default_value("graphsage"),
        "graph neural network model (graphsage, gcn)")
      ("gnn.train", po::bool_switch(&gnn_options.train)->default_value(false),
        "training or inference")
      ("gnn.query_gen_mode", po::value<std::string>(&gnn_options.query_gen_mode)->default_value("random"),
        "query generation mode (random, adjacent, file)")
      ("gnn.query_file", po::value<std::string>(&gnn_options.query_file)->default_value(""),
        "path to query file (only used when query_gen_mode is file)")
      ("gnn.num_queries", po::value<uint32_t>(&gnn_options.num_queries)->default_value(1000000),
        "number of queries to generate")
      ("gnn.num_layers", po::value<uint32_t>(&gnn_options.num_layers)->default_value(3),
        "number of layers")
      ("gnn.samples_per_layer", po::value<std::string>(&gnn_options.s_samples_per_layer)->default_value("10,10,10"),
        "number of samples per layer (e.g. 10,10,10)")
      ("gnn.node_feature_dim", po::value<uint32_t>(&gnn_options.node_feature_dim)->default_value(256),
        "node feature dimension")
    ;

    po::options_description flashgnn("FlashGNN Options");
    flashgnn.add_options()
      ("flashgnn.seed", po::value<uint32_t>(&flashgnn_options.seed)->default_value(2333),
        "random seed")
      ("flashgnn.query_gen_path", po::value<std::string>(&flashgnn_options.query_gen_path)->required(),
        "path to query generation results")
      ("flashgnn.batch_forming_mode", po::value<std::string>(&flashgnn_options.batch_forming_mode)->default_value("static"),
        "batch generation mode (static, dynamic)")
      ("flashgnn.lock_root_threshold", po::value<uint32_t>(&flashgnn_options.lock_root_threshold)->default_value(20),
        "if the number of child nodes of a root node is greater than this threshold, lock the root node")
      ("flashgnn.query_overlap_level", po::value<std::string>(&flashgnn_options.query_overlap_level)->default_value("serial"),
        "query overlapping level in each batch (serial, parallel)")
      ("flashgnn.dataflow_scheduling", po::value<std::string>(&flashgnn_options.dataflow_scheduling)->default_value("naive"),
        "dataflow scheduling strategy (naive, flash-aware)")
      ("flashgnn.reuse_sampling_results", po::bool_switch(&flashgnn_options.reuse_sampling_results)->default_value(false),
        "reuse sampling results")
      ("flashgnn.batch_size", po::value<uint32_t>(&flashgnn_options.batch_size)->default_value(1000),
        "number of queries to process in a batch")
      ("flashgnn.cache_capacity", po::value<uint64_t>(&flashgnn_options.cache_capacity)->default_value(65536),
        "capacity of node feature cache in bytes")
      ("flashgnn.aggregation_latency", po::value<uint64_t>(&flashgnn_options.aggregation_latency)->default_value(8),
        "latency of aggregation")
      ("flashgnn.num_combiners", po::value<uint32_t>(&flashgnn_options.num_combiners)->default_value(2),
        "number of combiners")
      ("flashgnn.mac_op_latency", po::value<uint64_t>(&flashgnn_options.mac_op_latency)->default_value(6),
        "latency of an MAC operation")
      ("flashgnn.num_sampler_task_entries", po::value<uint32_t>(&flashgnn_options.num_sampler_task_entries)->default_value(1024),
        "number of sampler task entries")
      ("flashgnn.num_processor_task_entries", po::value<uint32_t>(&flashgnn_options.num_processor_task_entries)->default_value(1024),
        "number of processor task entries")
      ("flashgnn.sampling_cache_size", po::value<uint32_t>(&flashgnn_options.sampling_cache_size)->default_value(10000),
        "size of sampling cache in number of nodes")
      ("flashgnn.ready_cache_size", po::value<uint32_t>(&flashgnn_options.ready_cache_size)->default_value(10000),
        "size of ready cache in number of nodes")
      ("flashgnn.separate_islands", po::bool_switch(&flashgnn_options.separate_islands)->default_value(false),
        "separate input node features in different islands to different flash pages")
      ("flashgnn.subpe.enabled", po::bool_switch(&flashgnn_options.subpe_options.enabled)->default_value(false),
        "enable subPEs' functionality")
      ("flashgnn.subpe.buffer_capacity", po::value<uint64_t>(&flashgnn_options.subpe_options.buffer_capacity)->default_value(262144),
        "capacity of subPE buffer in bytes")
      ("flashgnn.subpe.bandwidth", po::value<float>(&flashgnn_options.subpe_options.bandwidth)->default_value(128),
        "subPE buffer bandwidth in GB/s")
      ("flashgnn.subpe.aggregation_latency", po::value<uint64_t>(&flashgnn_options.subpe_options.aggregation_latency)->default_value(16),
        "latency of aggregation in a subPE")
      ("flashgnn.subpe.num_task_entries", po::value<uint32_t>(&flashgnn_options.subpe_options.num_task_entries)->default_value(4),
        "number of task entries for a subPE")
    ;

    po::options_description all_options;
    all_options.add(log);
    all_options.add(graph);
    all_options.add(ssd);
    all_options.add(dram);
    all_options.add(gnn);
    all_options.add(flashgnn);
    
    po::variables_map vm;
    try {
      po::store(po::parse_command_line(argc, argv, all_options), vm);
      po::notify(vm);
      if(vm.count("help")) { std::cout << all_options << std::endl; return false; }
    } catch (std::exception& e) {
      if(vm.count("help")) std::cout << all_options << std::endl;
      else                 std::cerr << e.what() << std::endl;
      return false;
    } catch (...) {
      std::cerr << "Unknown error!" << std::endl;
      return false;
    }

    if(!ssd_options.config_path.compare("")) {
      ssd_options.config_path = fmt::format("{}/config_{}.xml", ssd_options.dir, ssd_options.config);
    }
    if(!ssd_options.workload_path.compare("")) {
      ssd_options.workload_path = fmt::format("{}/workload_{}.xml", ssd_options.dir, ssd_options.config);
    }

    if(!gnn_options.model.compare("graphsage")) {
      std::vector<std::string> t_samples_per_layer;
      boost::algorithm::trim(gnn_options.s_samples_per_layer);
      boost::algorithm::split(t_samples_per_layer, gnn_options.s_samples_per_layer, boost::is_any_of(","));
      for(const auto& t : t_samples_per_layer) {
        gnn_options.samples_per_layer.push_back(std::stol(t));
      }
      gnn_options.num_layers = gnn_options.samples_per_layer.size();
      if(gnn_options.num_layers < 2) {
        std::cerr << fmt::format("There should be at least 2 layers, but only got {}.", gnn_options.num_layers) << std::endl;
        return false;
      }

      gnn_options.num_nodes_per_subgraph = 1;
      for(uint32_t depth = 1; depth <= gnn_options.num_layers; ++depth) {
        uint32_t val = 1;
        for(uint32_t d = 0; d < depth; ++d) {
          val *= gnn_options.samples_per_layer.at(d);
        }
        gnn_options.num_nodes_per_subgraph += val;
      }

    } else if(!gnn_options.model.compare("gcn")) {
      gnn_options.s_samples_per_layer = "";
      if(gnn_options.num_layers == 0) {
        std::cerr << "There should be at least 1 layer." << std::endl;
        return false;
      }

      gnn_options.num_nodes_per_subgraph = 1;
    } else {
      std::cerr << fmt::format("Unknown model: {}.", gnn_options.model) << std::endl;
      return false;
    }

    if(!gnn_options.model.compare("gcn") && !flashgnn_options.reuse_sampling_results) {
      std::cerr << "GCN model requires enabling reuse sampling results." << std::endl;
      return false;
    }

    /*if(!flashgnn_options.batch_forming_mode.compare("dynamic") && flashgnn_options.reuse_sampling_results) {
      std::cerr << "Dynamic batch forming mode for GraphSage model requires disabling reuse sampling results." << std::endl;
      return false;
    }*/

    /*if(!flashgnn_options.batch_forming_mode.compare("dynamic") && !flashgnn_options.query_overlap_level.compare("serial")) {
      std::cerr << "Dynamic batch forming mode requires query overlap level to be parallel." << std::endl;
      return false;
    }*/

    if(!flashgnn_options.query_overlap_level.compare("serial") && !dram_options.split_space) {
      std::cerr << "Serial query overlap level requires splitting DRAM space." << std::endl;
      return false;
    }

    return true;
  }

  std::string to_string() const {
    std::string log_str = fmt::format(
      "[log]\n"
      "level = {}\n",
      log_options.level
    );

    std::string graph_str = fmt::format(
      "[graph]\n"
      "path = {}\n"
      "block_size = {}\n",
      graph_options.path,
      graph_options.block_size
    );

    std::string ssd_str = fmt::format(
      "[ssd]\n"
      "dir = {}\n"
      "config = {}\n"
      "config_path = {}\n"
      "workload_path = {}\n"
      "buffer_capacity = {}\n"
      "discard_re_data = {}\n",
      ssd_options.dir,
      ssd_options.config,
      ssd_options.config_path,
      ssd_options.workload_path,
      ssd_options.buffer_capacity,
      ssd_options.discard_re_data
    );

    std::string dram_str = fmt::format(
      "[dram]\n"
      "use_ideal_memory = {}\n"
      "sure_to_hit = {}\n"
      "capacity = {}\n"
      "bandwidth = {}\n"
      "split_space = {}\n"
      "edge_lists_capacity = {}\n"
      "node_features_capacity = {}\n",
      dram_options.use_ideal_memory,
      dram_options.sure_to_hit,
      dram_options.capacity,
      dram_options.bandwidth,
      dram_options.split_space,
      dram_options.edge_lists_capacity,
      dram_options.node_features_capacity
    );

    std::string gnn_str = fmt::format(
      "[gnn]\n"
      "model = {}\n"
      "train = {}\n"
      "query_gen_mode = {}\n"
      "query_file = {}\n"
      "num_queries = {}\n"
      "num_layers = {}\n"
      "samples_per_layer = {}\n"
      "node_feature_dim = {}\n",
      gnn_options.model,
      gnn_options.train,
      gnn_options.query_gen_mode,
      gnn_options.query_file,
      gnn_options.num_queries,
      gnn_options.num_layers,
      gnn_options.s_samples_per_layer,
      gnn_options.node_feature_dim
    );

    std::string flashgnn_str = fmt::format(
      "[flashgnn]\n"
      "seed = {}\n"
      "query_gen_path = {}\n"
      "batch_forming_mode = {}\n"
      "lock_root_threshold = {}\n"
      "query_overlap_level = {}\n"
      "dataflow_scheduling = {}\n"
      "reuse_sampling_results = {}\n"
      "batch_size = {}\n"
      "cache_capacity = {}\n"
      "aggregation_latency = {}\n"
      "num_combiners = {}\n"
      "mac_op_latency = {}\n"
      "num_sampler_task_entries = {}\n"
      "num_processor_task_entries = {}\n"
      "sampling_cache_size = {}\n"
      "ready_cache_size = {}\n"
      "separate_islands = {}\n"
      "subpe.enabled = {}\n"
      "subpe.buffer_capacity = {}\n"
      "subpe.bandwidth = {}\n"
      "subpe.aggregation_latency = {}\n"
      "subpe.num_task_entries = {}\n",
      flashgnn_options.seed,
      flashgnn_options.query_gen_path,
      flashgnn_options.batch_forming_mode,
      flashgnn_options.lock_root_threshold,
      flashgnn_options.query_overlap_level,
      flashgnn_options.dataflow_scheduling,
      flashgnn_options.reuse_sampling_results,
      flashgnn_options.batch_size,
      flashgnn_options.cache_capacity,
      flashgnn_options.aggregation_latency,
      flashgnn_options.num_combiners,
      flashgnn_options.mac_op_latency,
      flashgnn_options.num_sampler_task_entries,
      flashgnn_options.num_processor_task_entries,
      flashgnn_options.sampling_cache_size,
      flashgnn_options.ready_cache_size,
      flashgnn_options.separate_islands,
      flashgnn_options.subpe_options.enabled,
      flashgnn_options.subpe_options.buffer_capacity,
      flashgnn_options.subpe_options.bandwidth,
      flashgnn_options.subpe_options.aggregation_latency,
      flashgnn_options.subpe_options.num_task_entries
    );

    return fmt::format("{}\n{}\n{}\n{}\n{}\n{}\n", log_str, graph_str, ssd_str, dram_str, gnn_str, flashgnn_str);
  }
};

};

#endif
