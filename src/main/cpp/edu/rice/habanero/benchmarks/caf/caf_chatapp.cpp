#include <algorithm>
#include <chrono>
#include <random>

#include "benchmark_runner.hpp"
#include "pseudo_random.hpp"

// types
using client_map = std::unordered_map<std::uint64_t, caf::actor>;
using friend_set = std::unordered_set<caf::actor>;
using chat_set = std::unordered_set<caf::actor>;
using client_set = std::unordered_set<caf::actor>;

// messages
using post_atom = caf::atom_constant<caf::atom("post")>;
using acknowledge_atom = caf::atom_constant<caf::atom("acknowledg")>;
using join_atom = caf::atom_constant<caf::atom("join")>;
using leave_atom = caf::atom_constant<caf::atom("leave")>;
using left_atom = caf::atom_constant<caf::atom("left")>;
using befriend_atom = caf::atom_constant<caf::atom("befriend")>;
using logout_atom = caf::atom_constant<caf::atom("logout")>;
using invite_atom = caf::atom_constant<caf::atom("invite")>;
using online_atom = caf::atom_constant<caf::atom("online")>;
using offline_atom = caf::atom_constant<caf::atom("offline")>;
using forward_atom = caf::atom_constant<caf::atom("forward")>;
using act_atom = caf::atom_constant<caf::atom("act")>;
using completed_atom = caf::atom_constant<caf::atom("completed")>;
using set_poker_atom = caf::atom_constant<caf::atom("set_poker")>;
using login_atom = caf::atom_constant<caf::atom("login")>;
using status_atom = caf::atom_constant<caf::atom("status")>;
using finished_atom = caf::atom_constant<caf::atom("finished")>;
using broadcast_atom = caf::atom_constant<caf::atom("broadcast")>;
using confirm_atom = caf::atom_constant<caf::atom("confirm")>;
using invitations_atom = caf::atom_constant<caf::atom("invitatio")>;

enum action { post, leave, invite, compute, none };

// util
class dice_roll {
public:
  dice_roll(std::uint64_t seed) : random_(seed) {
  }

  bool apply(std::uint64_t probability) {
    return random_.next_uint(100) < static_cast<std::uint32_t>(probability);
  }

private:
  pseudo_random random_;
};

std::uint64_t fibonacci(std::uint8_t x) {
  if (x == 0 || x == 1) {
    return x;
  } else {
    auto j = x / 2;
    auto fib_j = fibonacci(j);
    auto fib_i = fibonacci(j - 1);

    if (x % 2 == 0) {
      return fib_j * (fib_j + (fib_i * 2));
    } else if (x % 4 == 1) {
      return (((fib_j * 2) + fib_i) * ((fib_j * 2) - fib_i)) + 2;
    } else {
      return (((fib_j * 2) + fib_i) * ((fib_j * 2) - fib_i)) - 2;
    }
  }
}

struct behavior_factory {
  behavior_factory() = default;

  behavior_factory(uint64_t compute, uint64_t post, uint64_t leave,
                   uint64_t invite)
    : compute(compute), post(post), leave(leave), invite(invite) {
  }

  behavior_factory(const behavior_factory& f) = default;

  action apply() {
    auto dice
      = dice_roll(std::chrono::steady_clock::now().time_since_epoch().count());
    auto next_action = action::none;

    if (dice.apply(compute)) {
      next_action = action::compute;
    } else if (dice.apply(post)) {
      next_action = action::post;
    } else if (dice.apply(leave)) {
      next_action = action::leave;
    } else if (dice.apply(invite)) {
      next_action = action::invite;
    }

    return next_action;
  }

  std::uint64_t compute;
  std::uint64_t post;
  std::uint64_t leave;
  std::uint64_t invite;
};

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, behavior_factory& x) {
  return f(caf::meta::type_name("behavior_factory"), x.compute, x.post, x.leave,
           x.invite);
}

struct chat_state {
  client_set members;
  size_t acknowledgements;
  size_t invitations;
};

caf::behavior chat(caf::stateful_actor<chat_state>* self) {
  return {
    [=](post_atom, std::vector<std::uint8_t>& payload, caf::actor& directory) {
      auto& s = self->state;
      s.acknowledgements = s.members.size();
      if (s.members.empty()) {
        self->send(directory, completed_atom::value);
      } else {
        for (auto& member : s.members) {
          self->send(member, forward_atom::value, self, payload, directory);
        }
      }
    },
    [=](acknowledge_atom, caf::actor& directory) {
      auto& s = self->state;
      s.acknowledgements--;
      if (s.acknowledgements == 1) { // TODO check with pony: why ==1?
        self->send(directory, completed_atom::value);
      }
    },
    [=](invitations_atom, size_t invitations) {
      self->state.invitations = invitations;
    },
    [=](join_atom, caf::actor& client, caf::actor& directory) {
      auto& s = self->state;
      s.members.emplace(client);
      s.invitations--;
      if (s.invitations == 0) { // TODO check with pony: why ==1?
        self->send(directory, completed_atom::value);
      }
    },
    [=](leave_atom, caf::actor& client, bool did_logout) {
      auto& s = self->state;
      s.members.erase(client);
      self->send(client, left_atom::value, self, did_logout);
      if (s.members.empty()) {
        self->quit(); // TODO check with pony: stop actor here?
      }
    },
    [=](leave_atom, caf::actor& client, bool did_logout,
        caf::actor& directory) {
      auto& s = self->state;
      s.members.erase(client);
      self->send(client, left_atom::value, self, did_logout, directory);
      if (s.members.empty()) {
        self->quit(); // TODO check with pony: stop actor here?
      }
    },
  };
}

struct client_state {
  std::uint64_t id;
  friend_set friends;
  chat_set chats;
  caf::actor directory;
};

caf::behavior client(caf::stateful_actor<client_state>* self, std::uint64_t id,
                     caf::actor directory) {
  self->state.id = id;
  self->state.directory = std::move(directory);

  return {
    [=](befriend_atom, caf::actor& client) {
      self->state.friends.emplace(client);
    },
    [=](logout_atom) {
      auto& s = self->state;
      if (not s.chats.empty()) {
        for (auto& chat : s.chats) {
          self->send(chat, leave_atom::value, self, true);
        }
      } else {
        self->send(s.directory, left_atom::value, s.id);
        self->quit(); // TODO check with pony: stop actor here?
      }
    },
    [=](left_atom, caf::actor& chat, bool did_logout) {
      auto& s = self->state;
      s.chats.erase(chat);

      if (s.chats.empty() && did_logout) {
        self->send(s.directory, left_atom::value, s.id);
        self->quit(); // TODO check with pony: stop actor here?
      }
    },
    [=](left_atom, caf::actor& chat, bool did_logout, caf::actor& directory) {
      auto& s = self->state;
      self->send(directory, completed_atom::value);
      s.chats.erase(chat);

      if (s.chats.empty() && did_logout) {
        self->send(s.directory, left_atom::value, s.id);
        self->quit(); // TODO check with pony: stop actor here?
      }
    },
    [=](invite_atom, caf::actor& chat, caf::actor& directory) {
      self->state.chats.emplace(chat);
      self->send(chat, join_atom::value, self, directory);
    },
    [=](online_atom, std::uint64_t id) {
      // nop
    },
    [=](offline_atom, std::uint64_t id) {
      // nop
    },
    [=](forward_atom, caf::actor& chat, std::vector<std::uint8_t>& payload,
        caf::actor& directory) {
      self->send(chat, acknowledge_atom::value, directory);
    },
    [=](act_atom, behavior_factory& behavior) {
      auto& s = self->state;
      // caf::aout(self) << s.chats.size() << std::endl; //TODO check with pony:
      // size always zero
      caf::actor next_chat;
      if (s.chats.empty()) {
        next_chat
          = self->spawn(chat); // TODO check with pony: emplace in chats?
      } else {
        size_t index = static_cast<size_t>(pseudo_random(42).next_ulong())
                       % s.chats.size();
        next_chat = *std::next(s.chats.begin(), index);
      }

      switch (behavior.apply()) {
        case action::post:
          // caf::aout(self) << s.id << ": post" << std::endl;
          self->send(next_chat, post_atom::value, std::vector<std::uint8_t>(),
                     s.directory);
          break;
        case action::leave:
          // caf::aout(self) << s.id << ": leave" << std::endl;
          self->send(next_chat, leave_atom::value, self, false, s.directory);
          break;
        case action::compute:
          // caf::aout(self) << s.id << ": compute" << std::endl;
          fibonacci(35);
          self->send(s.directory, completed_atom::value);
          break;
        case action::none:
          // caf::aout(self) << s.id << ": none" << std::endl;
          self->send(s.directory, completed_atom::value);
          break;
        case action::invite:
          // caf::aout(self) << s.id << ": invite" << std::endl;
          auto created = self->spawn(chat);
          std::vector<caf::actor> f;
          for (auto& i : s.friends) {
            f.push_back(i);
          }
          auto rng = std::default_random_engine{42};
          std::shuffle(f.begin(), f.end(), rng);
          f.insert(f.begin(), self);

          size_t invitations = 1;
          if (not s.friends.empty()) {
            invitations = static_cast<size_t>(rng()) % s.friends.size();
            if (invitations == 0) {
              invitations = 1;
            }
          }

          self->send(created, invitations_atom::value, invitations);

          for (size_t i = 0; i < invitations; ++i) {
            self->send(f[i], invite_atom::value, created, s.directory);
          }
          break;
      }
    },
  };
}

struct directory_state {
  size_t id;
  client_map clients;
  pseudo_random random{42};
  std::atomic_size_t completions{0};
  caf::actor poker = nullptr;
};

caf::behavior directory(caf::stateful_actor<directory_state>* self, size_t id) {
  self->state.id = id;
  return {
    [=](set_poker_atom, caf::actor& poker) { self->state.poker = poker; },
    [=](login_atom, std::uint64_t id) {
      auto new_client = self->spawn(client, id, self);
      auto& s = self->state;
      s.clients.emplace(id, new_client);
      for (auto& client : s.clients) {
        if (s.random.next_uint(100) < 10) {
          self->send(client.second, befriend_atom::value, new_client);
          self->send(new_client, befriend_atom::value, client.second);
        }
      }
    },
    [=](logout_atom, std::uint64_t id) {
      self->send(self->state.clients.at(id), logout_atom::value);
    },
    [=](status_atom, std::uint64_t id, caf::actor& requestor) {
      auto& s = self->state;
      try {
        s.clients.at(id);
        self->send(requestor, online_atom::value, id);
      } catch (std::out_of_range& e) {
        self->send(requestor, offline_atom::value, id);
      }
    },
    [=](left_atom, std::uint64_t id) {
      auto& s = self->state;
      s.clients.erase(id);
      if (s.clients.empty()) {
        if (s.poker != nullptr) {
          self->send(s.poker, finished_atom::value);
          s.poker = nullptr;
          self->quit(); // TODO check with pony: stop actor here?
        }
      }
    },
    [=](broadcast_atom, behavior_factory factory) {
      auto& s = self->state;
      s.completions += s.clients.size();
      for (auto& client : s.clients) {
        self->send(client.second, act_atom::value, factory);
      }
    },
    [=](completed_atom) {
      auto& s = self->state;
      s.completions--;
      if (s.completions == 1) { // TODO check with pony: why ==1?
        if (s.poker != nullptr) {
          self->send(s.poker,
                     confirm_atom::value); // TODO check with pony: here maybe
                                           // poke.completions?
        }
      }
    },
  };
}

struct poker_state {
  std::uint64_t clients;
  size_t logouts;
  size_t confirmations;
  std::uint64_t turns;
  std::vector<caf::actor> directories;
  behavior_factory factory;
  caf::actor bench;
};

caf::behavior poker(caf::stateful_actor<poker_state>* self,
                    std::uint64_t clients, std::uint64_t turns,
                    std::vector<caf::actor> directories,
                    behavior_factory factory, caf::actor bench) {
  auto& s = self->state;
  s.clients = clients;
  s.logouts = directories.size();
  s.confirmations = directories.size(); // TODO check with pony: why * turns?
  s.turns = turns;
  s.directories = directories;
  s.factory = factory;
  s.bench = bench;

  for (auto& d : s.directories) {
    self->send(d, set_poker_atom::value, self);
  }

  for (std::uint64_t client = 0; client < s.clients; ++client) {
    auto index = static_cast<size_t>(client) % s.directories.size();
    self->send(s.directories.at(index), login_atom::value, client);
  }

  for (; s.turns > 0; s.turns -= 1) {
    for (auto& directory : s.directories) {
      self->send(directory, broadcast_atom::value, s.factory);
    }
  }
  caf::aout(self) << "poker started" << std::endl;
  return {
    [=](confirm_atom) { // TODO when pony calls this?
      auto& s = self->state;
      s.confirmations--;
      if (s.confirmations == 1) { // TODO check with pony: why ==1?
        /**
         * The logout/teardown phase may only happen
         * after we know that all turns have been
         * carried out completely.
         */
        for (std::uint64_t client_id = 0; client_id < s.clients; ++client_id) {
          auto index = static_cast<size_t>(client_id) % s.directories.size();
          self->send(s.directories.at(index), logout_atom::value, client_id);
        }
      }
    },
    [=](finished_atom) {
      auto& s = self->state;
      s.logouts--;
      if (s.logouts == 1) { // TODO check with pony: why ==1?
        caf::aout(self) << "finish poker" << std::endl;
        self->send(s.bench, finished_atom::value);
        self->quit();
      }
    },
  };
}

struct config : caf::actor_system_config {
  std::uint64_t clients = 256; // 256;
  std::uint64_t turns = 20;    // 20;
  config() {
    add_message_type<behavior_factory>("behavior_factory");
  }
};

class bench : public benchmark {
public:
  void print_arg_info() const override {
    printf(benchmark_runner::arg_output_format(), "C (num of clients)",
           std::to_string(cfg_.clients).c_str());
    printf(benchmark_runner::arg_output_format(), "T (num of turns)",
           std::to_string(cfg_.turns).c_str());
  }

  void initialize(caf::actor_system_config::string_list& args) override {
    std::ifstream ini{ini_file(args)};
    cfg_.parse(args, ini);
  }

  void run_iteration() override {
    caf::actor_system system{cfg_};
    auto factory = behavior_factory(compute_, post_, leave_, invite_);
    std::vector<caf::actor> dirs;
    for (size_t i = 0; i < directories_; ++i) {
      dirs.push_back(system.spawn(directory, i));
    }
    {
      caf::scoped_actor self{system};
      self->spawn(poker, cfg_.clients, cfg_.turns, dirs, factory, self);
      self->receive([=](finished_atom) { std::cout << "done" << std::endl; });
    }
    system.await_all_actors_done();
  }

protected:
  const char* current_file() const override {
    return __FILE__;
  }

private:
  config cfg_;
  const size_t directories_ = 16; // 16;
  const std::uint64_t compute_ = 50;
  const std::uint64_t post_ = 80;
  const std::uint64_t leave_ = 25;
  const std::uint64_t invite_ = 25;
};

int main(int argc, char** argv) {
  benchmark_runner br;
  br.run_benchmark(argc, argv, bench{});
}
