#include <chrono>
#include <iostream>
#include <queue>
#include <stack>

#include "compressed_graph.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "uncompressed_graph.h"

ABSL_FLAG(std::string, input_path, "", "Input file path.");
ABSL_FLAG(bool, dfs, false, "Run DFS (as opposed to BFS)?");
ABSL_FLAG(bool, print, false, "Print node indices during traversal?");

void TimedBFS(zuckerli::CompressedGraph graph, bool print) {
  std::queue<uint32_t> nodes;
  std::vector<bool> visited(graph.size(), false);
  int num_visited = 0;

  std::cout << "BFS..." << std::endl;
  auto t_start = std::chrono::high_resolution_clock::now();
  for (uint32_t root = 0; root < graph.size(); root++) {
    if (visited[root]) continue;
    nodes.push(root);
    visited[root] = true;
    ++num_visited;
    while (!nodes.empty()) {
      uint32_t current_node = nodes.front();
      nodes.pop();
      if (print) std::cout << current_node << " ";
      for (uint32_t neighbour : graph.Neighbours(current_node)) {
        if (!visited[neighbour]) {
          nodes.push(neighbour);
          visited[neighbour] = true;
          ++num_visited;
        }
      }
    }
  }
  auto t_stop = std::chrono::high_resolution_clock::now();
  if (print) std::cout << std::endl;
  std::cout
      << "Wall time elapsed: "
      << std::chrono::duration<double, std::milli>(t_stop - t_start).count()
      << " ms" << std::endl;
}

void TimedDFS(zuckerli::CompressedGraph graph, bool print) {
  std::stack<uint32_t> nodes;
  std::vector<bool> visited(graph.size(), false);
  int num_visited = 0;

  std::cout << "DFS..." << std::endl;
  auto t_start = std::chrono::high_resolution_clock::now();
  size_t count = 0;
  for (uint32_t root = 0; root < graph.size(); root++) {
    if (visited[root]) continue;
    count++;
    nodes.push(root);
    visited[root] = true;
    ++num_visited;
    while (!nodes.empty()) {
      uint32_t current_node = nodes.top();
      nodes.pop();
      if (print) std::cout << current_node << " ";
      for (uint32_t neighbour : graph.Neighbours(current_node)) {
        if (!visited[neighbour]) {
          nodes.push(neighbour);
          visited[neighbour] = true;
          ++num_visited;
        }
      }
    }
  }
  auto t_stop = std::chrono::high_resolution_clock::now();
  if (print) std::cout << std::endl;
  std::cout
      << "Wall time elapsed: "
      << std::chrono::duration<double, std::milli>(t_stop - t_start).count()
      << " ms" << std::endl;
}

int main(int argc, char* argv[]) {
  absl::ParseCommandLine(argc, argv);
  zuckerli::CompressedGraph graph(absl::GetFlag(FLAGS_input_path));
  std::cout << "This graph has " << graph.size() << " nodes." << std::endl;
  if (absl::GetFlag(FLAGS_dfs)) {
    TimedDFS(graph, absl::GetFlag(FLAGS_print));
  } else {
    TimedBFS(graph, absl::GetFlag(FLAGS_print));
  }
  return 0;
}
