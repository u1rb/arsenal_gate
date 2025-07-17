#include <boost/pfr.hpp>
#include <fmt/format.h>
#include <string>

struct Person {
  std::string name;
  int age;
  double height;
};

template <typename T>
void print_struct(const T &obj, const std::string &title) {
  fmt::print("{} (fields: {})\n", title, boost::pfr::tuple_size_v<T>);
  boost::pfr::for_each_field(
      obj, [](const auto &field) { fmt::print("  {}\n", field); });
}

int main() {
  Person alice{"Alice", 30, 165.5};
  Person bob{"Bob", 25, 175.0};

  print_struct(alice, "Alice");
  print_struct(bob, "Bob");

  fmt::print("\nAlice == Bob: {}\n", boost::pfr::eq_fields(alice, bob));

  auto [name, age, height] = boost::pfr::structure_tie(alice);
  fmt::print("\nStructured binding: {} is {} years old\n", name, age);

  return 0;
}