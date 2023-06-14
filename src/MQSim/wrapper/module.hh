#ifndef FG_MODULE_H
#define FG_MODULE_H

#include <random>

namespace FlashGNN {

class Module {
protected:
  uint64_t _last_cycle;
  uint64_t _cycle;
  float _tCK;
  float _clock_ns;

  std::default_random_engine _rand_eng;

  uint32_t _epoch;
  uint64_t _last_epoch_cycle;

public:
  Module(float tCK = 1.0) : _last_cycle(0), _cycle(0), _tCK(tCK), _clock_ns(0.0), _epoch(0), _last_epoch_cycle(0) {}
  virtual ~Module() {}

  virtual void tick() = 0;

  virtual bool busy() const = 0;

  virtual uint64_t get_cycle() const { return _cycle; }
  float get_tCK() const { return _tCK; }
  virtual float get_clock_ns() const { return _clock_ns; }

  virtual void set_cycle(uint64_t cycle) { _cycle = cycle; _clock_ns = cycle * _tCK; }
  void set_tCK(float tCK) { _tCK = tCK; }

  void set_rand_eng(uint32_t seed) { _rand_eng = std::default_random_engine(seed); }
};

};

#endif
