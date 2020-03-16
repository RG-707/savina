#include <fstream>
#include <iostream>
#include <stdlib.h>

#include "benchmark_runner.hpp"

CAF_BEGIN_TYPE_ID_BLOCK(fjcreate, first_custom_type_id)

CAF_ADD_ATOM(fjcreate, msg_atom)

CAF_END_TYPE_ID_BLOCK(fjcreate)

using namespace std;
using namespace caf;

class config : public actor_system_config {
public:
  int n = 40000;

  config() {
    init_global_meta_objects<fjcreate_type_ids>();
    opt_group{custom_options_, "global"}.add(n, "nnn,n", "num of workers");
  }
};

void perform_computation(double theta) {
  double sint = sin(theta);
  double res = sint * sint;
  if (res <= 0) {
    throw string("Benchmark exited with unrealistic res value "
                 + to_string(res));
  }
}

behavior fork_join_actor(event_based_actor* self) {
  return {[=](msg_atom) {
    perform_computation(37.2);
    self->quit();
  }};
}

class bench : public benchmark {
public:
  void print_arg_info() const override {
    printf(benchmark_runner::arg_output_format(), "N (num workers)",
           to_string(cfg_.n).c_str());
  }

  void initialize(caf::actor_system_config::string_list& args) override {
    std::ifstream ini{ini_file(args)};
    cfg_.parse(args, ini);
  }

  void run_iteration() override {
    actor_system system{cfg_};
    for (int i = 0; i < cfg_.n; ++i) {
      auto fj_runner = system.spawn(fork_join_actor);
      anon_send(fj_runner, msg_atom_v);
    }
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
