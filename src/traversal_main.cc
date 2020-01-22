#include <chrono>
#include <iostream>
#include <queue>
#include <stack>

#include "uncompressed_graph.h"

void TimedBFS(const zuckerli::UncompressedGraph& graph, bool print) {
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
      for (uint32_t neighbour : graph.neighs(current_node)) {
        if (!visited[neighbour]) {
          nodes.push(neighbour);
          visited[neighbour] = true;
          ++num_visited;
        }
      }
    }
  }
  auto t_stop = std::chrono::high_resolution_clock::now();
  std::cout
      << "Wall time elapsed: "
      << std::chrono::duration<double, std::milli>(t_stop - t_start).count()
      << " ms" << std::endl;
}

void TimedDFS(const zuckerli::UncompressedGraph& graph, bool print) {
  std::stack<uint32_t> nodes;
  std::vector<bool> visited(graph.size(), false);
  int num_visited = 0;

  std::cout << "DFS..." << std::endl;
  auto t_start = std::chrono::high_resolution_clock::now();
  for (uint32_t root = 0; root < graph.size(); root++) {
    if (visited[root]) continue;
    nodes.push(root);
    visited[root] = true;
    ++num_visited;
    while (!nodes.empty()) {
      uint32_t current_node = nodes.top();
      nodes.pop();
      if (print) std::cout << current_node << " ";
      for (uint32_t neighbour : graph.neighs(current_node)) {
        if (!visited[neighbour]) {
          nodes.push(neighbour);
          visited[neighbour] = true;
          ++num_visited;
        }
      }
    }
  }
  auto t_stop = std::chrono::high_resolution_clock::now();
  std::cout
      << "Wall time elapsed: "
      << std::chrono::duration<double, std::milli>(t_stop - t_start).count()
      << " ms" << std::endl;
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cout << "Exactly one argument required (graph filename)." << std::endl;
    return 0;
  }

  zuckerli::UncompressedGraph graph(argv[1]);
  std::cout << "This graph has " << graph.size() << " nodes." << std::endl;
  TimedBFS(graph, false);
  TimedDFS(graph, false);
  return 0;
}
