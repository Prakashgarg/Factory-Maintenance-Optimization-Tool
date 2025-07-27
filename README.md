# Factory Maintenance Optimization Tool (FMSS)

Factory Maintenance Optimization Tool (FMSS) is a console-based C++ simulation program designed to help factory managers determine the optimal number of maintenance workers (called "adjusters") needed to minimize downtime while controlling labor costs.

## Project Objective

Factories have machines that randomly fail over time. Each failure causes downtime, which affects production. However, hiring too many maintenance workers leads to underutilization and wasted cost. FMSS helps balance this trade-off by simulating:

- Random machine failures based on Mean Time To Failure (MTTF)
- Repair time and repair queues
- Adjuster utilization and workload
- Machine uptime and downtime statistics

The goal is to help managers answer:
- How many adjusters are needed?
- Which machines fail the most?
- What is the impact of different maintenance strategies?

## Features

- Add multiple machine types with different failure and repair characteristics
- Add adjuster groups with skill mapping to machine types
- Run year-based simulations with daily updates
- Tracks:
  - Machine uptime and breakdowns
  - Adjuster usage and idle time
  - Repair queue statistics
- Console-based menu system with detailed reporting

## Project Structure

- `Simulator.cpp` — Core C++ source code with full simulation logic
- `Simulator.exe` — Optional Windows executable (for direct use)
- `.vscode/` — VS Code configuration for C++ development (optional)

## How to Run

### Compile and Run (G++):
```bash
g++ -o Simulator Simulator.cpp
./Simulator
