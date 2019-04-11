#include <atomic>
#include <caf/atom.hpp>
#include <queue>

#include "benchmark_runner.hpp"
#include "pseudo_random.hpp"

using namespace std;
using namespace caf;

class config : public actor_system_config {
public:
  int n = 5000;
  int w = 1000;
  int apr = 1000;
  int ahr = 1000;

  config() {
    opt_group{custom_options_, "global"}
      .add(n, "nnn,n", "number of haircuts")
      .add(w, "www,w", "waiting room size")
      .add(apr, "apr,p", "average production rate")
      .add(ahr, "ahr,h", "average haircut rate");
  }
};

using start_atom = atom_constant<atom("start")>;
using exit_atom = atom_constant<atom("exit")>;
using enter_atom = atom_constant<atom("enter")>;
using full_atom = atom_constant<atom("full")>;
using next_atom = atom_constant<atom("next")>;
using wait_atom = atom_constant<atom("wait")>;
using done_atom = atom_constant<atom("done")>;
using returned_atom = atom_constant<atom("returned")>;

int busy_wait(const int limit) {
  int test = 0;

  for (int k = 0; k < limit; k++) {
    rand();
    test++;
  }

  return test;
}

struct waiting_room_sates {
  atomic_bool barber_asleep;
  queue<actor> waiting_customers;
};

behavior waiting_room_actor(stateful_actor<waiting_room_sates>* self,
                            size_t capacity, const actor& barber) {
  self->state.barber_asleep = true;

  return {[=](enter_atom, actor customer) {
            CAF_LOG_DEBUG("Waitingroom: Customer " << customer << " enter.");
            auto& s = self->state;
            if (s.waiting_customers.size() == capacity) {
              CAF_LOG_DEBUG("Waitingroom: Full!");
              self->send(customer, full_atom::value);
            } else {
              s.waiting_customers.push(customer);
              if (s.barber_asleep) {
                s.barber_asleep = false;
                self->send(self, next_atom::value);
              } else {
                self->send(customer, wait_atom::value);
              }
            }
          },
          [=](next_atom) {
            auto& s = self->state;
            if (s.waiting_customers.empty()) {
              CAF_LOG_DEBUG("Waitingroom: Barber sleep.");
              s.barber_asleep = true;
              self->send(barber, wait_atom::value);
            } else {
              CAF_LOG_DEBUG("Waitingroom: Customer to barber.");
              auto customer = s.waiting_customers.front();
              self->send(barber, enter_atom::value, customer, self);
              s.waiting_customers.pop();
            }
          },
          [=](exit_atom) {
            CAF_LOG_DEBUG("Waitingroom: Exiting.");
            self->send(barber, exit_atom::value);
            self->quit();
          }};
}

struct barber_states {
  pseudo_random random;
};

behavior barber_actor(stateful_actor<barber_states>* self, int ahr) {
  return {[=](enter_atom, actor customer, actor room) {
            CAF_LOG_DEBUG("Barber: Processing customer " << customer);
            auto& s = self->state;
            self->send(customer, start_atom::value);
            busy_wait(s.random.next_int(ahr) + 10);
            self->send(customer, done_atom::value);
            self->send(room, next_atom::value);
          },
          [=](wait_atom) {
            CAF_LOG_DEBUG("Barber: No customers. Going to have a sleep");
          },
          [=](exit_atom) {
            CAF_LOG_DEBUG("Barber: Exiting.");
            self->quit();
          }};
}

behavior customer_actor(event_based_actor* self, int id,
                        const actor& customer_factory) {
  return {
    [=](full_atom) {
      CAF_LOG_DEBUG("Customer:" << id
                                << " The waiting room is full. I am leaving.");
      self->send(customer_factory, returned_atom::value, self);
    },
    [=](wait_atom) { CAF_LOG_DEBUG("Customer:" << id << " I will wait."); },
    [=](start_atom) {
      CAF_LOG_DEBUG("Customer:" << id << " I am now being served.");
    },
    [=](done_atom) {
      CAF_LOG_DEBUG("Customer:" << id << " I have been served.");
      self->send(customer_factory, done_atom::value);
      self->quit();
    }};
}

struct customer_factory_states {
  pseudo_random random;
  atomic_long haircuts_so_far;
};

behavior customer_factory_actor(stateful_actor<customer_factory_states>* self,
                                atomic_long* counter, int apr, int haircuts,
                                const actor& room) {
  self->state.haircuts_so_far = 0;

  return {[=](start_atom) {
            CAF_LOG_DEBUG("Factory: Start spawning customers.");
            for (int i = 0; i < haircuts; ++i) {
              auto customer =
                self->spawn(customer_actor, counter->fetch_add(1), self);
              self->send(room, enter_atom::value, customer);
              busy_wait(self->state.random.next_int(apr) + 10);
            }
            CAF_LOG_DEBUG("Factory: Finished spawning customers.");
          },
          [=](returned_atom, actor customer) {
            CAF_LOG_DEBUG("Factory: Customers " << customer << " returned.");
            counter->fetch_add(1);
            self->send(room, enter_atom::value, customer);
          },
          [=](done_atom) {
            auto& s = self->state;
            s.haircuts_so_far++;
            CAF_LOG_DEBUG("Factory: Haircut number " << s.haircuts_so_far
                                                     << " done.");
            if (s.haircuts_so_far == haircuts) {
              self->send(room, exit_atom::value);
              self->quit();
            }
          }};
}

class bench : public benchmark {
public:
  void print_arg_info() const override {
    printf(benchmark_runner::arg_output_format(), "N (num haircuts)",
           to_string(cfg_.n).c_str());
    printf(benchmark_runner::arg_output_format(), "W (waiting room size)",
           to_string(cfg_.w).c_str());
    printf(benchmark_runner::arg_output_format(), "APR (production rate)",
           to_string(cfg_.apr).c_str());
    printf(benchmark_runner::arg_output_format(), "AHR (haircut rate)",
           to_string(cfg_.ahr).c_str());
  }

  void initialize(caf::actor_system_config::string_list& args) override {
    std::ifstream ini{ini_file(args)};
    cfg_.parse(args, ini);
  }

  void run_iteration() override {
    actor_system system{cfg_};
    atomic_long counter{0};
    auto barber = system.spawn(barber_actor, cfg_.ahr);
    auto room = system.spawn(waiting_room_actor, cfg_.w, barber);
    auto factory =
      system.spawn(customer_factory_actor, &counter, cfg_.apr, cfg_.n, room);

    anon_send(factory, start_atom::value);

    system.await_all_actors_done();
    cout << "Total attempts: " << counter << endl;
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
