#include <atomic>
#include <iostream>
#include <vector>

#include "gmp.h"
#include "gmpxx.h"

#include "benchmark_runner.hpp"

CAF_BEGIN_TYPE_ID_BLOCK(piprecision, first_custom_type_id)

CAF_ADD_TYPE_ID(piprecision, (mpf_class))
CAF_ADD_ATOM(piprecision, start_atom)
CAF_ADD_ATOM(piprecision, stop_atom)
CAF_ADD_ATOM(piprecision, work_atom)
CAF_ADD_ATOM(piprecision, result_atom)

CAF_END_TYPE_ID_BLOCK(piprecision)

using namespace std;
using namespace caf;

class config : public actor_system_config {
public:
  int num_workers = 20;
  int precision = 5000;

  config() {
    init_global_meta_objects<piprecision_type_ids>();
    opt_group{custom_options_, "global"}
      .add(num_workers, "nnn,n", "number of workers")
      .add(precision, "ppp,p", "precision");
  }
};

//TODO Inspector for mpf_class

/// Formula: http://mathworld.wolfram.com/BBPFormula.html
mpf_class calculate_bbp_term(const int precision, const int k) {
  const mpf_class one(1, precision);
  const mpf_class two(2, precision);
  const mpf_class four(4, precision);
  const mpf_class sixteen(16, precision);
  const int eight_k = 8 * k;
  mpf_class term = four / mpf_class(eight_k + 1, precision);
  term -= (two / mpf_class(eight_k + 4, precision));
  term -= (one / mpf_class(eight_k + 5, precision));
  term -= (one / mpf_class(eight_k + 6, precision));
  mpf_class sixteen_pow_k(1, precision);
  mpf_pow_ui(sixteen_pow_k.get_mpf_t(), sixteen.get_mpf_t(), k);
  term /= sixteen_pow_k;
  return term;
}

behavior worker_actor(event_based_actor* self, const actor& master, int id) {
  return {[=](stop_atom) {
            self->send(master, stop_atom_v);
            CAF_LOG_DEBUG("worker: " << id << " quitting");
            self->quit();
          },
          [=](work_atom, const int precision, const int term) {
            auto result = calculate_bbp_term(precision, term);
            self->send(master, result_atom_v, result, id);
          }};
}

struct master_states {
  vector<actor> workers;
  mpf_class result = 0;
  mpf_class tolerance = 1;
  atomic_int num_worker_terminated{0};
  int num_term_requested = 0;
  int num_term_received = 0;
  bool stop_requests = false;
};

behavior master_actor(stateful_actor<master_states>* self,
                      const int num_workers, const int precision) {
  self->state.tolerance.set_prec(precision);
  self->state.result.set_prec(precision);
  self->state.tolerance = "1e-26";

  // onPostStart()
  for (int i = 0; i < num_workers; ++i) {
    self->state.workers.emplace_back(self->spawn(worker_actor, self, i));
  }
  return {[=](result_atom, const mpf_class result, int worker_id) {
            auto& s = self->state;
            s.num_term_received++;
            s.result += result;
            if (cmp(s.result, s.tolerance) <= 0) {
              s.stop_requests = true;
            }
            if (!s.stop_requests) {
              self->send(s.workers.at(worker_id), work_atom_v, precision,
                         s.num_term_requested);
              s.num_term_requested++;
            }
            if (s.num_term_received == s.num_term_requested) {
              for (auto& worker : s.workers) {
                self->send(worker, stop_atom_v);
              }
            }
          },
          [=](stop_atom) {
            auto& s = self->state;
            s.num_worker_terminated++;
            if (s.num_worker_terminated == num_workers) {
              CAF_LOG_DEBUG("result: " << s.result);
              self->quit();
            }
          },
          [=](start_atom) {
            auto& s = self->state;
            for (int t = 0; t < min(precision, 10 * num_workers); ++t) {
              self->send(s.workers.at(t % num_workers), work_atom_v,
                         precision, s.num_term_requested);
              s.num_term_requested++;
            }
          }};
}

class bench : public benchmark {
public:
  void print_arg_info() const override {
    printf(benchmark_runner::arg_output_format(), "N (num workers)",
           to_string(cfg_.num_workers).c_str());
    printf(benchmark_runner::arg_output_format(), "P (precision)",
           to_string(cfg_.precision).c_str());
  }

  void initialize(caf::actor_system_config::string_list& args) override {
    std::ifstream ini{ini_file(args)};
    cfg_.parse(args, ini);
  }

  void run_iteration() override {
    actor_system system{cfg_};

    auto master = system.spawn(master_actor, cfg_.num_workers, cfg_.precision);
    anon_send(master, start_atom_v);
    system.await_all_actors_done();
    cout << "done" << endl;
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
