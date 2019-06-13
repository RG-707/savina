#include <algorithm>
#include <atomic>
#include <fstream>
#include <iostream>
#include <queue>
#include <unordered_map>
#include <vector>

#include "benchmark_runner.hpp"
#include "pseudo_random.hpp"

using namespace std;
using namespace caf;

class config : public actor_system_config {
public:
  int priority_granularity = 8;
  int num_worker = 20;
  int grid_size = 30;
  int priorities = 30;
  int threshold = 1024;

  config() {
    opt_group{custom_options_, "global"}
      .add(priority_granularity, "pgg,g", "priority granularity")
      .add(num_worker, "www,w", "num worker")
      .add(grid_size, "gss,s", "grind size")
      .add(priorities, "ppp,p", "priorities")
      .add(threshold, "ttt,t", "threshold");
  }
};

using start_atom = atom_constant<atom("start")>;
using exit_atom = atom_constant<atom("exit")>;
using work_atom = atom_constant<atom("work")>;
using received_atom = atom_constant<atom("received")>;
using done_atom = atom_constant<atom("done")>;
using stop_atom = atom_constant<atom("stop")>;

class grid_node;
bool operator==(const grid_node& x, const grid_node& y);

class grid_node {
public:
  grid_node(int id, int i, int j, int k) : id_(id), i_(i), j_(j), k_(k) {
    distance_from_root_ = id == 0 ? 0 : -1;
    parent_in_path_.store(nullptr);
  }

  int id() const {
    return id_;
  }

  int i() const {
    return i_;
  }

  int j() const {
    return j_;
  }

  int k() const {
    return k_;
  }

  grid_node* parent_in_path() const {
    return parent_in_path_.load();
  }

  size_t num_neighbors() {
    return neighbors_.size();
  }

  shared_ptr<grid_node> neighbor(const int n) {
    return neighbors_.at(n);
  }

  bool set_parent(grid_node* node) {
    grid_node* expected = nullptr;
    bool success = parent_in_path_.compare_exchange_weak(
      expected, node, memory_order_release, memory_order_relaxed);
    if (success) {
      distance_from_root_
        = node->distance_from_root_ + static_cast<int>(distance_from(*node));
    }
    return success;
  }

  double distance_from(const grid_node& node) {
    int i_diff = i_ - node.i();
    int j_diff = j_ - node.j();
    int k_diff = k_ - node.k();
    return sqrt((i_diff * i_diff) + (j_diff * j_diff) + (k_diff * k_diff));
  }

  bool add_neighbor(shared_ptr<grid_node> node) {
    if (*node.get() == *this) {
      return false;
    }
    if (find(neighbors_.begin(), neighbors_.end(), node) == neighbors_.end()) {
      neighbors_.push_back(node);
      return true;
    }
    return false;
  }

private:
  int id_;
  int i_;
  int j_;
  int k_;
  atomic<grid_node*> parent_in_path_;
  vector<shared_ptr<grid_node>> neighbors_;

  // fields used in computing distance
  int distance_from_root_;
};

bool operator==(const grid_node& x, const grid_node& y) {
  return x.id() == y.id() && x.i() == y.i() && x.k() == y.k() && x.j() == y.j();
}

void busy_wait() {
  for (int i = 0; i < 100; i++) {
    rand();
  }
}

behavior worker_actor(event_based_actor* self, const actor& master, int id,
                      const int threshold,
                      const unordered_map<int, shared_ptr<grid_node>>& grid) {
  return {[=](work_atom, int node_id, int target_id) {
            CAF_LOG_DEBUG("worker " << id << ": begin work");
            queue<int> work_queue;
            work_queue.push(node_id);
            int nodes_processed = 0;
            while (!work_queue.empty() && nodes_processed < threshold) {
              CAF_LOG_DEBUG("worker " << id << ": while begin");
              nodes_processed++;
              busy_wait();
              auto& loop_node_id = work_queue.front();
              work_queue.pop();
              auto& loop_node = grid.at(loop_node_id);
              for (size_t i = 0; i < loop_node->num_neighbors(); ++i) {
                auto loop_neighbor = loop_node->neighbor(i);
                bool success = loop_neighbor->set_parent(loop_node.get());
                if (success) {
                  if (loop_neighbor == grid.at(target_id)) {
                    CAF_LOG_DEBUG("worker" << id << ": work done");
                    self->send(master, done_atom::value);
                    self->send(master, received_atom::value);
                    return;
                  } else {
                    work_queue.push(loop_neighbor->id());
                  }
                }
              }
            }

            while (!work_queue.empty()) {
              CAF_LOG_DEBUG("worker " << id << ": get new work");
              self->send(master, work_atom::value, work_queue.front(),
                         target_id);
              work_queue.pop();
            }
            CAF_LOG_DEBUG("worker " << id << ": work done");
            self->send(master, received_atom::value);
          },
          [=](stop_atom) {
            CAF_LOG_DEBUG("worker " << id << ": quiting");
            self->send(master, stop_atom::value);
            self->quit();
          }};
}

struct master_states {
  vector<actor> worker;
  int num_worker_terminated = 0;
  int num_work_send = 0;
  int num_work_completed = 0;
};

behavior master_actor(stateful_actor<master_states>* self, const int num_worker,
                      const int threshold,
                      const unordered_map<int, shared_ptr<grid_node>>& grid) {
  // onPostStart()
  for (int i = 0; i < num_worker; ++i) {
    self->state.worker.emplace_back(
      self->spawn(worker_actor, self, i, threshold, grid));
  }
  return {[=](work_atom, int node_id, int target_id) {
            CAF_LOG_DEBUG("master: work");
            auto& s = self->state;
            int worker_index = s.num_work_send % num_worker;
            s.num_work_send++;
            self->send(s.worker.at(worker_index), work_atom::value, node_id,
                       target_id);
          },
          [=](received_atom) {
            CAF_LOG_DEBUG("master: receive");
            auto& s = self->state;
            s.num_work_completed++;
            if (s.num_work_completed == s.num_work_send) {
              for (auto& worker : s.worker) {
                self->send(worker, stop_atom::value);
              }
            }
          },
          [=](done_atom) {
            CAF_LOG_DEBUG("master: done");
            for (auto& worker : self->state.worker) {
              self->send(worker, stop_atom::value);
            }
          },
          [=](stop_atom) {
            CAF_LOG_DEBUG("master: stop");
            auto& s = self->state;
            s.num_worker_terminated++;
            if (s.num_worker_terminated == num_worker) {
              CAF_LOG_DEBUG("master: quiting");
              self->quit();
            }
          }};
}

class bench : public benchmark {
public:
  void print_arg_info() const override {
    printf(benchmark_runner::arg_output_format(), "Granularity",
           to_string(cfg_.priority_granularity).c_str());
    printf(benchmark_runner::arg_output_format(), "Num Workers",
           to_string(cfg_.num_worker).c_str());
    printf(benchmark_runner::arg_output_format(), "Grid Size",
           to_string(cfg_.grid_size).c_str());
    printf(benchmark_runner::arg_output_format(), "Priorities",
           to_string(cfg_.priorities).c_str());
    printf(benchmark_runner::arg_output_format(), "Threshold",
           to_string(cfg_.threshold).c_str());
  }

  void initialize(caf::actor_system_config::string_list& args) override {
    std::ifstream ini{ini_file(args)};
    cfg_.parse(args, ini);

    CAF_LOG_DEBUG("begin initialize");

    // init grid data
    int id = 0;
    for (int i = 0; i < cfg_.grid_size; ++i) {
      for (int j = 0; j < cfg_.grid_size; ++j) {
        for (int k = 0; k < cfg_.grid_size; ++k) {
          all_nodes_ptr_.emplace(id, make_shared<grid_node>(id, i, j, k));
          ++id;
        }
      }
    }
    pseudo_random random(123456L);
    for (int node_id = 0; node_id < id; ++node_id) {
      int iter_count = 0;
      int neighbor_count = 0;
      for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
          for (int k = 0; k < 2; ++k) {
            ++iter_count;
            if (iter_count == 1 || iter_count == 8) {
              continue;
            }

            bool add_neighbor = (iter_count == 7 && neighbor_count == 0)
                                || random.next_boolean();
            if (add_neighbor) {
              int new_i
                = min(cfg_.grid_size - 1, all_nodes_ptr_.at(node_id)->i() + i);
              int new_j
                = min(cfg_.grid_size - 1, all_nodes_ptr_.at(node_id)->j() + j);
              int new_k
                = min(cfg_.grid_size - 1, all_nodes_ptr_.at(node_id)->k() + k);
              int new_id = (cfg_.grid_size * cfg_.grid_size * new_i)
                           + (cfg_.grid_size * new_j) + new_k;
              if (all_nodes_ptr_.at(node_id)->add_neighbor(
                    all_nodes_ptr_.at(new_id))) {
                ++neighbor_count;
              }
            }
          }
        }
      }
    }
    CAF_LOG_DEBUG("end initialize");
  }

  void run_iteration() override {
    CAF_LOG_DEBUG("start iteration");
    actor_system system{cfg_};
    auto master = system.spawn(master_actor, cfg_.num_worker, cfg_.threshold,
                               all_nodes_ptr_);
    // generate target
    auto axis_val = static_cast<int>(0.80 * cfg_.grid_size);
    int target_id = (axis_val * cfg_.grid_size * cfg_.grid_size)
                    + (axis_val * cfg_.grid_size) + axis_val;
    anon_send(master, work_atom::value, 0, target_id);
    system.await_all_actors_done();

    int nodes_processed = 1;
    for (auto& node_pair : all_nodes_ptr_) {
      if (node_pair.second.get()->parent_in_path() != nullptr) {
        nodes_processed++;
      }
    }
    cout << "Nodes processed: " << nodes_processed << endl;
  }

protected:
  const char* current_file() const override {
    return __FILE__;
  }

private:
  config cfg_;
  unordered_map<int, shared_ptr<grid_node>> all_nodes_ptr_;
};

int main(int argc, char** argv) {
  benchmark_runner br;
  br.run_benchmark(argc, argv, bench{});
}
