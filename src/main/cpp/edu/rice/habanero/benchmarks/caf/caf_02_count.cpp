#include <fstream>
#include <iostream>

#include "benchmark_runner.hpp"

CAF_BEGIN_TYPE_ID_BLOCK(count, first_custom_type_id)

CAF_ADD_TYPE_ID(count, (retrieve_msg))
CAF_ADD_TYPE_ID(count, (result_msg))
CAF_ADD_ATOM(count, increment_atom)

CAF_END_TYPE_ID_BLOCK(count)

using namespace std;
using namespace caf;

class config : public actor_system_config {
public:
  static int n; //= 1e6;

  config() {
    init_global_meta_objects<count_type_ids>();
    opt_group{custom_options_, "global"}.add(n, "num,n", "number of messages");
  }
};
int config::n = 1e6;

struct retrieve_msg {
  actor sender;
};
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, retrieve_msg& x) {
  return f(caf::meta::type_name("retrieve_msg"), x.sender);
}

struct result_msg {
  int result;
};
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, result_msg& x) {
  return f(caf::meta::type_name("result_msg"), x.result);
}

behavior producer_actor(event_based_actor* self, actor counting) {
  return {[=](increment_atom) {
    for (int i = 0; i < config::n; ++i) {
      self->send(counting, increment_atom_v);
    }
    self->send(counting, retrieve_msg{actor_cast<actor>(self)});
  },
          [=](result_msg& m) {
            auto result = m.result;
            if (result != config::n) {
              cout << "ERROR: expected: " << config::n << ", found: " << result
                   << endl;
            } else {
              cout << "SUCCESS! received: " << result << endl;
            }
          }};
}

behavior counting_actor(stateful_actor<int>* self) {
  self->state = 0;
  return {
    [=](increment_atom) { ++self->state; },
    [=](retrieve_msg& m) { self->send(m.sender, result_msg{self->state}); }};
}

class bench : public benchmark {
public:
  void print_arg_info() const override {
    printf(benchmark_runner::arg_output_format(), "N (num messages)",
           to_string(cfg_.n).c_str());
  }

  void initialize(caf::actor_system_config::string_list& args) override {
    std::ifstream ini{ini_file(args)};
    cfg_.parse(args, ini);
  }

  void run_iteration() override {
    actor_system system{cfg_};
    auto counting = system.spawn(counting_actor);
    auto producer = system.spawn(producer_actor, counting);
    anon_send(producer, increment_atom_v);
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
