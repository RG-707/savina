#include <fstream>
#include <iostream>

#include "benchmark.hpp"
#include "benchmark_runner.hpp"

CAF_BEGIN_TYPE_ID_BLOCK(pingpong, first_custom_type_id)

  CAF_ADD_ATOM(pingpong, start_msg_atom)
  CAF_ADD_ATOM(pingpong, ping_msg_atom)
  CAF_ADD_ATOM(pingpong, pong_msg_atom)
  CAF_ADD_ATOM(pingpong, stop_msg_atom)

CAF_END_TYPE_ID_BLOCK(pingpong)

using namespace std;
using namespace caf;

class config : public actor_system_config {
public:
  int n = 40000;

  config() {
    init_global_meta_objects<pingpong_type_ids>();
    opt_group{custom_options_, "global"}.add(n, "num,n",
                                             "number of ping-pongs");
  }
};

behavior ping_actor(stateful_actor<int>* self, int count, actor pong) {
  self->state = count;
  return {[=](start_msg_atom) {
            self->send(pong, ping_msg_atom_v, self);
            --self->state;
          },
          [=](ping_msg_atom) {
            self->send(pong, ping_msg_atom_v, self);
            --self->state;
          },
          [=](pong_msg_atom) {
            if (self->state > 0) {
              self->send(self, ping_msg_atom_v);
            } else {
              self->send(pong, stop_msg_atom_v);
              self->quit();
            }
          }};
}

behavior pong_actor(stateful_actor<int>* self) {
  self->state = 0;
  return {[=](ping_msg_atom, actor sender) {
            ++self->state;
          self->send(sender, pong_msg_atom_v);
          },
          [=](stop_msg_atom) { self->quit(); }};
}

void starter_actor(event_based_actor* self, const config* cfg) {
  auto pong = self->spawn(pong_actor);
  auto ping = self->spawn(ping_actor, cfg->n, pong);
  self->send(ping, start_msg_atom_v);
}

class bench : public benchmark {
public:
  void print_arg_info() const override {
    printf(benchmark_runner::arg_output_format(), "n (num pings)",
           to_string(cfg_.n).c_str());
  }

  void initialize(caf::actor_system_config::string_list& args) override {
    std::ifstream ini{ini_file(args)};
    cfg_.parse(args, ini);
  }

  void run_iteration() override {
    actor_system system{cfg_};
    system.spawn(starter_actor, &cfg_);
  }

protected:
  const char* current_file() const override {
    return __FILE__;
  }

private:
  config cfg_;
};

int main(int argc, char** argv) {
  benchmark_runner br;
  br.run_benchmark(argc, argv, bench{});
}
