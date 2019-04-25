#include <atomic>
#include <caf/atom.hpp>

#include "benchmark_runner.hpp"
#include "pseudo_random.hpp"

using namespace std;
using namespace caf;

class config : public actor_system_config {
public:
  int max_nodes = 200000;
  int avg_comp_size = 500;
  int stdev_comp_size = 100;
  int binomial_param = 10;
  int urgent_node_percent = 50;

  config() {
    opt_group{custom_options_, "global"}
      .add(max_nodes, "max,m", "maximum nodes")
      .add(avg_comp_size, "avg,a", "average computation size")
      .add(stdev_comp_size, "std,s",
           "standard deviation of the computation size")
      .add(
        binomial_param, "bin,b",
        "binomial parameter: each node may have either 0 or binomial children")
      .add(urgent_node_percent, "urg,u", "percentage of urgent nodes");
  }
};

using start_atom = atom_constant<atom("start")>;
using exit_atom = atom_constant<atom("exit")>;
using print_info_atom = atom_constant<atom("print")>;
using get_id_atom = atom_constant<atom("get_id")>;
using generate_tree_atom = atom_constant<atom("gen_tree")>;
using update_grant_atom = atom_constant<atom("up_grant")>;
using try_generate_children_atom = atom_constant<atom("trygenchi")>;
using should_generate_children_atom = atom_constant<atom("sgc_msg")>;
using generate_children_atom = atom_constant<atom("gen_child")>;
using urgent_generate_children_atom = atom_constant<atom("urggenchi")>;
using traverse_atom = atom_constant<atom("traverse")>;
using urgent_traverse_atom = atom_constant<atom("utraverse")>;

int busy_wait(const int limit, const int dummy) {
  int test = 0;
  auto current = chrono::system_clock::now();
  for (int k = 0; k < dummy * limit; k++) {
    test++;
  }
  return test;
}

int get_next_normal(pseudo_random& ran, int pmean, int pdev) {
  int erg = 0;
  double temp = 0;
  while (erg <= 0) {
    temp = ran.next_gaussian() * pdev + pmean;
    erg = static_cast<int>(round(temp));
  }
  return erg;
}

struct node_actor_states {
  int urgent_child = 0;
  bool has_children = false;
  vector<actor> children;
  vector<bool> has_grant_children;
};

behavior node_actor(stateful_actor<node_actor_states>* self,
                    const actor& my_parent, const actor& my_root,
                    const int my_height, const int my_id,
                    const int my_computation_size, const bool is_urgent,
                    const int& binomial_param) {
  auto dummy = 40000;
  return {
    // This message is called by parent node, try to generate children of this
    // node. If the "getBoolean" message returns true, the node is allowed to
    // generate 'binomial_param' children.
    [=](try_generate_children_atom) {
      CAF_LOG_DEBUG("node_actor " << my_id << ": try generate children.");
      busy_wait(100, dummy);
      self->send(my_root, should_generate_children_atom::value, self,
                 my_height);
    },
    [=](generate_children_atom, const int current_id, const int comp_size) {
      CAF_LOG_DEBUG("node_actor " << my_id << ": generate children.");
      auto& s = self->state;
      int my_array_id = my_id % binomial_param;
      self->send(my_parent, update_grant_atom::value, my_array_id);
      int children_height = my_height + 1;
      for (int i = 0; i < binomial_param; ++i) {
        s.has_grant_children.emplace_back(false);
        s.children.emplace_back(self->spawn(node_actor, self, my_root,
                                            children_height, current_id + i,
                                            comp_size, false, binomial_param));
      }
      s.has_children = true;
      for (int j = 0; j < binomial_param; ++j) {
        self->send(s.children.at(j), try_generate_children_atom::value);
      }
    },
    [=](urgent_generate_children_atom, const int urgent_child_id,
        const int current_id, const int comp_size) {
      CAF_LOG_DEBUG("node_actor " << my_id << ": urgent generate children.");
      auto& s = self->state;
      int my_array_id = my_id % binomial_param;
      self->send(my_parent, update_grant_atom::value, my_array_id);
      int children_height = my_height + 1;
      s.urgent_child = urgent_child_id;
      for (int i = 0; i < binomial_param; ++i) {
        s.children.emplace_back(self->spawn(
          node_actor, self, my_root, children_height, current_id + i, comp_size,
          i == s.urgent_child, binomial_param));
      }
    },
    // This message is called by a child node to indicate that it has children.
    [=](update_grant_atom, const int child_id) {
      CAF_LOG_DEBUG("node_actor " << my_id << ": update grant.");
      self->state.has_grant_children.at(child_id) = true;
    },
    // This message is called by parent while doing a traverse.
    [=](traverse_atom) {
      CAF_LOG_DEBUG("node_actor " << my_id << ": traverse.");
      auto& s = self->state;
      busy_wait(my_computation_size, dummy);
      if (s.has_children) {
        for (int i = 0; i < binomial_param; ++i) {
          self->send(s.children.at(i), traverse_atom::value);
        }
      }
    },
    // This message is called by parent while doing traverse, if this node is an
    // urgent node.
    [=](urgent_traverse_atom) {
      CAF_LOG_DEBUG("node_actor " << my_id << ": urgent traverse.");
      auto& s = self->state;
      busy_wait(my_computation_size, dummy);
      if (s.has_children) {
        if (s.urgent_child != -1) {
          for (int i = 0; i < binomial_param; ++i) {
            if (i != s.urgent_child) {
              self->send(s.children.at(i), traverse_atom::value);
            } else {
              self->send(s.children.at(s.urgent_child),
                         urgent_traverse_atom::value);
            }
          }
        } else {
          for (int i = 0; i < binomial_param; ++i) {
            self->send(s.children.at(i), traverse_atom::value);
          }
        }
        if (is_urgent) {
          aout(self) << "urgent traverse node " << my_id << " "
                     << chrono::system_clock::now().time_since_epoch().count()
                     << endl;
        } else {
          aout(self) << my_id << " "
                     << chrono::system_clock::now().time_since_epoch().count()
                     << endl;
        }
      }
    },
    [=](print_info_atom) {
      CAF_LOG_DEBUG("node_actor " << my_id << ": print info.");
      auto& s = self->state;
      if (is_urgent) {
        aout(self) << "Urgent......";
      }
      if (s.has_children) {
        aout(self) << my_id << " " << my_computation_size
                   << "  children starts " << endl;
        for (int i = 0; i < binomial_param; ++i) {
          self->send(s.children.at(i), print_info_atom::value);
        }
      } else {
        aout(self) << my_id << " " << my_computation_size << endl;
      }
    },
    [=](get_id_atom) {
      // Removed, never called.
    },
    [=](exit_atom) {
      CAF_LOG_DEBUG("node_actor " << my_id << ": exited.");
      auto& s = self->state;
      if (s.has_children) {
        for (int i = 0; i < binomial_param; ++i) {
          self->send(s.children.at(i), exit_atom::value);
        }
      }
      self->quit();
    }};
}

struct root_actor_states {
  pseudo_random random;
  int height = 1;
  int size = 1;
  bool traversed = false;
  bool final_size_printed = false;
  vector<actor> children;
  vector<bool> has_grant_children;
};

behavior root_actor(stateful_actor<root_actor_states>* self,
                    const int& avg_comp_size, const int& stdev_comp_size,
                    const int& binomial_param, const int& max_nodes,
                    const int& urgent_node_percent) {

  return {[=](generate_tree_atom) {
            CAF_LOG_DEBUG("root_actor: generate tree.");
            auto& s = self->state;
            s.height += 1;
            int computation_size
              = get_next_normal(s.random, avg_comp_size, stdev_comp_size);
            for (int i = 0; i < binomial_param; ++i) {
              s.has_grant_children.emplace_back(false);
              s.children.emplace_back(
                self->spawn(node_actor, self, self, s.height, s.size + i,
                            computation_size, false, binomial_param));
            }
            s.size += binomial_param;
            for (int j = 0; j < binomial_param; ++j) {
              self->send(s.children.at(j), try_generate_children_atom::value);
            }
          },
          [=](update_grant_atom, int id) {
            CAF_LOG_DEBUG("root_actor: update grant.");
            self->state.has_grant_children.at(id) = true;
          },
          [=](should_generate_children_atom, actor sender, int child_height) {
            CAF_LOG_DEBUG("root_actor: should generate children.");
            auto& s = self->state;
            if (s.size + binomial_param <= max_nodes) {
              if (s.random.next_boolean()) {
                int child_comp
                  = get_next_normal(s.random, avg_comp_size, stdev_comp_size);
                int random_int = s.random.next_int(100);
                if (random_int > urgent_node_percent) {
                  self->send(sender, generate_children_atom::value, s.size,
                             child_comp);
                } else {
                  self->send(sender, urgent_generate_children_atom::value,
                             s.random.next_int(binomial_param), s.size,
                             child_comp);
                }
                s.size += binomial_param;
                if (child_height + 1 > s.height) {
                  s.height = child_height + 1;
                }
              } else {
                if (child_height > s.height) {
                  s.height = child_height;
                }
              }
            } else {
              if (!s.final_size_printed) {
                aout(self) << "final size= " << s.size << endl;
                aout(self) << "final height= " << s.height << endl;
                s.final_size_printed = true;
              }
              if (!s.traversed) {
                s.traversed = true;
                for (int i = 0; i < binomial_param; ++i) {
                  self->send(s.children.at(i), traverse_atom::value);
                }
              }
              for (int j = 0; j < binomial_param; ++j) {
                self->send(s.children.at(j), exit_atom::value);
              }
              self->quit();
            }
          },
          [=](print_info_atom) {
            CAF_LOG_DEBUG("root_actor: print info.");
            for (int i = 0; i < binomial_param; ++i) {
              self->send(self->state.children.at(i), print_info_atom::value);
            }
          },
          [=](exit_atom) {
            CAF_LOG_DEBUG("root_actor: exited.");
            for (int i = 0; i < binomial_param; ++i) {
              self->send(self->state.children.at(i), exit_atom::value);
            }
            self->quit();
          }

  };
}

class bench : public benchmark {
public:
  void print_arg_info() const override {
    printf(benchmark_runner::arg_output_format(), "Max. nodes",
           to_string(cfg_.max_nodes).c_str());
    printf(benchmark_runner::arg_output_format(), "Avg. comp size",
           to_string(cfg_.avg_comp_size).c_str());
    printf(benchmark_runner::arg_output_format(), "Std. dev. comp size",
           to_string(cfg_.stdev_comp_size).c_str());
    printf(benchmark_runner::arg_output_format(), "Binomial Param",
           to_string(cfg_.binomial_param).c_str());
    printf(benchmark_runner::arg_output_format(), "Urgent node percent",
           to_string(cfg_.urgent_node_percent).c_str());
  }

  void initialize(caf::actor_system_config::string_list& args) override {
    std::ifstream ini{ini_file(args)};
    cfg_.parse(args, ini);
  }

  void run_iteration() override {
    actor_system system{cfg_};
    auto root = system.spawn(root_actor, cfg_.avg_comp_size,
                             cfg_.stdev_comp_size, cfg_.binomial_param,
                             cfg_.max_nodes, cfg_.urgent_node_percent);

    anon_send(root, generate_tree_atom::value);

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
