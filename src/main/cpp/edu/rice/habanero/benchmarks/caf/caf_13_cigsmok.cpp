#include <atomic>
#include <caf/atom.hpp>
#include <queue>

#include "benchmark_runner.hpp"
#include "pseudo_random.hpp"

using namespace std;
using namespace caf;

class config : public actor_system_config {
public:
  int r = 1000;
  int s = 200;

  config() {
    opt_group{custom_options_, "global"}
      .add(r, "rrr,r", "number of rounds")
      .add(s, "sss,s", "number of smokers / ingredients");
  }
};

using start_atom = atom_constant<atom("start")>;
using exit_atom = atom_constant<atom("exit")>;
using smoke_atom = atom_constant<atom("smoke")>;
using smoked_atom = atom_constant<atom("smoked")>;

int busy_wait(const int limit) {
  int test = 0;

  for (int k = 0; k < limit; k++) {
    rand();
    test++;
  }

  return test;
}

behavior smoker_actor(event_based_actor* self, const actor& arbiter) {
  return {[=](smoke_atom, int busy_wait_periode) {
            // notify arbiter that started smoking
            self->send(arbiter, smoked_atom::value);
            // now smoke cigarette
            busy_wait(busy_wait_periode);
          },
          [=](exit_atom) { self->quit(); }};
}

struct arbiter_actor_states {
  vector<actor> smoker_actors;
  pseudo_random random;
  atomic_long rounds_so_far;
};

behavior arbiter_actor(stateful_actor<arbiter_actor_states>* self,
                       int num_rounds, int num_smoker) {
  self->state.rounds_so_far = 0;
  self->state.random.set_seed(num_rounds * num_smoker);
  // onPostStart()
  {
    for (int i = 0; i < num_smoker; ++i) {
      self->state.smoker_actors.emplace_back(self->spawn(smoker_actor, self));
    }
  }

  return {[=](start_atom) {
            auto& s = self->state;
            // choose a random smoker to start smoking
            auto index = abs(s.random.next_int()) % num_smoker;
            auto busy_wait_periode = s.random.next_int(1000) + 10;
            self->send(s.smoker_actors.at(index), smoke_atom::value,
                       busy_wait_periode);
          },
          [=](smoked_atom) {
            auto& s = self->state;
            // resources are off the table, can place new ones on the table
            s.rounds_so_far++;
            if (s.rounds_so_far >= num_rounds) {
              // had enough, now exit
              for (auto& smoker : s.smoker_actors) {
                self->send(smoker, exit_atom::value);
              }
              self->quit();
            } else {
              // choose a random smoker to start smoking
              auto index = abs(s.random.next_int()) % num_smoker;
              auto busy_wait_periode = s.random.next_int(1000) + 10;
              self->send(s.smoker_actors.at(index), smoke_atom::value,
                         busy_wait_periode);
            }
          }};
}

class bench : public benchmark {
public:
  void print_arg_info() const override {
    printf(benchmark_runner::arg_output_format(), "R (num rounds)",
           to_string(cfg_.r).c_str());
    printf(benchmark_runner::arg_output_format(), "S (num smokers)",
           to_string(cfg_.s).c_str());
  }

  void initialize(caf::actor_system_config::string_list& args) override {
    std::ifstream ini{ini_file(args)};
    cfg_.parse(args, ini);
  }

  void run_iteration() override {
    actor_system system{cfg_};
    auto arbiter = system.spawn(arbiter_actor, cfg_.r, cfg_.s);

    anon_send(arbiter, start_atom::value);

    system.await_all_actors_done();
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
