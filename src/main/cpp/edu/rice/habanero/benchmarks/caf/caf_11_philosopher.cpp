#include <atomic>
#include <fstream>
#include <iostream>
#include <vector>

#include "benchmark_runner.hpp"

CAF_BEGIN_TYPE_ID_BLOCK(philosopher, first_custom_type_id)

CAF_ADD_ATOM(philosopher, start_atom)
CAF_ADD_ATOM(philosopher, exit_atom)
CAF_ADD_ATOM(philosopher, hungry_atom)
CAF_ADD_ATOM(philosopher, done_atom)
CAF_ADD_ATOM(philosopher, eat_atom)
CAF_ADD_ATOM(philosopher, denied_atom)

CAF_END_TYPE_ID_BLOCK(philosopher)

using namespace std;
using namespace caf;

class config : public actor_system_config {
public:
  int n = 20;
  int m = 10000;

  config() {
    init_global_meta_objects<philosopher_type_ids>();
    opt_group{custom_options_, "global"}
      .add(n, "nnn,n", "number of philosophers")
      .add(m, "mmm,m", "number of eating rounds");
  }
};


struct philosopher_states {
  long local_counter = 0;
  int rounds_so_far = 0;
};

behavior philosopher_actor(stateful_actor<philosopher_states>* self, int id,
                           int rounds, atomic_long* counter, actor arbitrator) {
  return {[=](denied_atom) {
            auto& s = self->state;
            ++s.local_counter;
            self->send(arbitrator, hungry_atom_v, id);
          },
          [=](eat_atom) {
            auto& s = self->state;
            ++s.rounds_so_far;
            counter->fetch_add(s.local_counter);
            self->send(arbitrator, done_atom_v, id);
            if (s.rounds_so_far < rounds) {
              self->send(self, start_atom_v);
            } else {
              self->send(arbitrator, exit_atom_v);
              self->quit();
            }
          },
          [=](start_atom) { self->send(arbitrator, hungry_atom_v, id); }};
}

struct arbitrator_actor_state {
  vector<bool> forks;
  int num_exit_philosophers = 0;
};

behavior arbitrator_actor(stateful_actor<arbitrator_actor_state>* self,
                          int num_forks) {
  auto& s = self->state;
  s.forks.reserve(num_forks);
  for (int i = 0; i < num_forks; ++i) {
    s.forks.push_back(false); // fork not used
  }
  return {[=](hungry_atom, int philosopher_id) {
            auto& s = self->state;
            auto left_fork = philosopher_id;
            auto right_fork = (philosopher_id + 1) % s.forks.size();
            if (s.forks[left_fork] || s.forks[right_fork]) {
              self->send(actor_cast<actor>(self->current_sender()),
                         denied_atom_v);
            } else {
              s.forks[left_fork] = true;
              s.forks[right_fork] = true;
              self->send(actor_cast<actor>(self->current_sender()),
                         eat_atom_v);
            }
          },
          [=](done_atom, int philosopher_id) {
            auto& s = self->state;
            auto left_fork = philosopher_id;
            auto right_fork = (philosopher_id + 1) % s.forks.size();
            s.forks[left_fork] = false;
            s.forks[right_fork] = false;
          },
          [=](exit_atom) {
            auto& s = self->state;
            ++s.num_exit_philosophers;
            if (num_forks == s.num_exit_philosophers) {
              self->quit();
            }
          }};
}

class bench : public benchmark {
public:
  void print_arg_info() const override {
    printf(benchmark_runner::arg_output_format(), "N (num philosophers)",
           to_string(cfg_.n).c_str());
    printf(benchmark_runner::arg_output_format(), "M (num eating rounds)",
           to_string(cfg_.m).c_str());
  }

  void initialize(caf::actor_system_config::string_list& args) override {
    std::ifstream ini{ini_file(args)};
    cfg_.parse(args, ini);
  }

  void run_iteration() override {
    actor_system system{cfg_};
    atomic_long counter{0};
    auto arbitrator = system.spawn(arbitrator_actor, cfg_.n);
    vector<actor> philosophers;
    philosophers.reserve(cfg_.n);
    for (int i = 0; i < cfg_.n; ++i) {
      philosophers.emplace_back(
        system.spawn(philosopher_actor, i, cfg_.m, &counter, arbitrator));
    }
    for (auto& loop_actor : philosophers) {
      anon_send(loop_actor, start_atom_v);
    }
    system.await_all_actors_done();
    cout << "Num retries: " << counter << endl;
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
