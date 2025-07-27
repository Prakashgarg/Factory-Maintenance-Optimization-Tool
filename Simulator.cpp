#include <iostream>
#include <string>
#include <vector>
#include <queue>
#include <map>
#include <limits>
#include <iomanip>
#include <stdexcept>
#include <random>
#include <algorithm>

using namespace std;

// ----------- Structs and Classes ------------

// Machine type info
struct MachineType {
    string name;
    int MTTF_days;      // mean time to failure in days
    int repair_time;    // repair days
    int quantity;       // number of machines
    MachineType() = default;
    MachineType(const string& n, int m, int r, int q) : name(n), MTTF_days(m), repair_time(r), quantity(q) {}
};

// Machine instance for simulation
struct MachineInstance {
    int group_index;
    int id_in_group;
    bool working;            // true = working, false = broken/repair
    int running_days;        // days machine worked since last repair/failure
    int repair_days;         // days spent repairing so far
    int failure_day;         // day count until next failure (randomized)

    MachineInstance(int group, int id)
        : group_index(group), id_in_group(id), working(true),
          running_days(0), repair_days(0), failure_day(-1) {}
};

// Adjuster group info
struct AdjusterGroup {
    string id;
    int count;
    vector<string> capable_machines; // machine types the group can service

    AdjusterGroup() = default;
    AdjusterGroup(const string& i, int c, const vector<string>& caps) : id(i), count(c), capable_machines(caps) {}
};

// Adjuster instance for simulation
struct AdjusterInstance {
    int group_index;
    int id_in_group;
    bool busy;
    int days_worked;            // days spent working on current repair
    int required_days;          // total repair days required for current job
    MachineInstance* current_machine;  // pointer to machine being repaired
    int total_busy_days;

    AdjusterInstance(int group_idx, int id)
        : group_index(group_idx), id_in_group(id), busy(false),
          days_worked(0), required_days(0), current_machine(nullptr), total_busy_days(0) {}
};

// Event for timeline logging
struct TimelineEvent {
    int day;
    string description;
    TimelineEvent(int d, const string& desc) : day(d), description(desc) {}
};


// ------------------- Helper input functions -------------------

void ignoreLine() {
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

int getIntInput(const string& prompt, int minVal, int maxVal) {
    int val;
    while (true) {
        cout << prompt;
        if (!(cin >> val)) {
            cout << "Invalid input. Please enter an integer.\n";
            cin.clear();
            ignoreLine();
            continue;
        }
        if (val < minVal || val > maxVal) {
            cout << "Input must be between " << minVal << " and " << maxVal << ".\n";
            ignoreLine();
            continue;
        }
        ignoreLine();
        return val;
    }
}

string getNonEmptyString(const string& prompt) {
    string s;
    while (true) {
        cout << prompt;
        getline(cin, s);
        if (s.empty()) {
            cout << "Input cannot be empty. Try again.\n";
            continue;
        }
        return s;
    }
}

bool contains(const vector<string>& vec, const string& val) {
    return find(vec.begin(), vec.end(), val) != vec.end();
}

// ------------------- Simulator Class -------------------

class FMSSimulator {
private:
    vector<MachineType> machine_types;
    vector<AdjusterGroup> adjuster_groups;

    vector<vector<MachineInstance>> machines; // per machine type group
    vector<vector<AdjusterInstance>> adjusters; // per adjuster group

    queue<MachineInstance*> repair_queue;

    int simulation_days = 0;

    // Random number generator
    default_random_engine rng;

    // Timeline events
    vector<TimelineEvent> timeline;

    // For max queue length tracking
    int max_queue_length = 0;

public:
    FMSSimulator() {
        rng.seed(random_device{}());
    }

    void addMachineType() {
        cout << "\n-- Add Machine Type --\n";
        string name = getNonEmptyString("Enter machine type name: ");
        for (const auto& mt : machine_types) {
            if (mt.name == name) {
                cout << "Machine type with this name already exists.\n";
                return;
            }
        }
        int mttf = getIntInput("Enter MTTF (days) (>=1): ", 1, 10000);
        int repair_time = getIntInput("Enter Repair Time (days) (>=1): ", 1, 10000);
        int quantity = getIntInput("Enter Quantity (1-1000): ", 1, 1000);

        machine_types.emplace_back(name, mttf, repair_time, quantity);
        cout << "Machine type \"" << name << "\" added successfully.\n";
    }

    void addAdjusterGroup() {
        if (machine_types.empty()) {
            cout << "Add at least one machine type before adding adjusters.\n";
            return;
        }
        cout << "\n-- Add Adjuster Group --\n";
        string id = getNonEmptyString("Enter Adjuster Group ID: ");
        for (const auto& ag : adjuster_groups) {
            if (ag.id == id) {
                cout << "Adjuster group with this ID already exists.\n";
                return;
            }
        }
        int count = getIntInput("Enter Number of Adjusters (1-1000): ", 1, 1000);

        cout << "Available machine types:\n";
        for (size_t i = 0; i < machine_types.size(); ++i) {
            cout << i + 1 << ". " << machine_types[i].name << "\n";
        }
        cout << "Select machine types serviced by this adjuster group (enter numbers separated by space):\n";

        vector<string> selected_machines;
        while (true) {
            cout << "Selection: ";
            string line;
            getline(cin, line);

            selected_machines.clear();
            size_t pos = 0;
            try {
                while (pos < line.size()) {
                    while (pos < line.size() && isspace(line[pos])) ++pos;
                    if (pos >= line.size()) break;
                    size_t endpos = pos;
                    while (endpos < line.size() && !isspace(line[endpos])) ++endpos;
                    string token = line.substr(pos, endpos - pos);
                    int sel = stoi(token);
                    if (sel < 1 || sel >(int)machine_types.size()) throw invalid_argument("Invalid number");
                    string m_name = machine_types[sel - 1].name;
                    if (!contains(selected_machines, m_name)) selected_machines.push_back(m_name);
                    pos = endpos;
                }
                if (selected_machines.empty()) throw invalid_argument("Empty selection");
                break;
            }
            catch (const exception&) {
                cout << "Invalid selection. Try again.\n";
            }
        }

        adjuster_groups.emplace_back(id, count, selected_machines);
        cout << "Adjuster group \"" << id << "\" added successfully.\n";
    }

    void initializeSimulation() {
        machines.clear();
        for (size_t i = 0; i < machine_types.size(); ++i) {
            vector<MachineInstance> group;
            for (int q = 0; q < machine_types[i].quantity; ++q) {
                MachineInstance m(i, q);
                // Assign randomized failure day
                int fail_day = randomizedFailureDay(machine_types[i].MTTF_days);
                m.failure_day = fail_day;
                group.push_back(m);
            }
            machines.push_back(move(group));
        }

        adjusters.clear();
        for (size_t i = 0; i < adjuster_groups.size(); ++i) {
            vector<AdjusterInstance> group;
            for (int q = 0; q < adjuster_groups[i].count; ++q) {
                group.emplace_back(i, q);
            }
            adjusters.push_back(move(group));
        }

        while (!repair_queue.empty()) repair_queue.pop();
        timeline.clear();
        max_queue_length = 0;

        cout << "\nSimulation initialized:\n  Machine types: " << machine_types.size()
            << "\n  Adjuster groups: " << adjuster_groups.size() << "\n";
    }

    int randomizedFailureDay(int mttf) {
        exponential_distribution<double> dist(1.0 / mttf);
        int day = static_cast<int>(dist(rng));
        if (day < 1) day = 1; // at least one day until failure
        return day;
    }

    bool canAdjusterServiceMachine(int adj_group_index, const string& machine_name) {
        for (const auto& m : adjuster_groups[adj_group_index].capable_machines) {
            if (m == machine_name) return true;
        }
        return false;
    }

    void runSimulation() {
        if (machine_types.empty()) {
            cout << "Error: Add at least one machine type before simulation.\n";
            return;
        }
        if (adjuster_groups.empty()) {
            cout << "Error: Add at least one adjuster group before simulation.\n";
            return;
        }

        int years = getIntInput("Enter number of years to simulate (>=1): ", 1, 1000);
        simulation_days = years * 365;

        initializeSimulation();

        cout << "\nStarting simulation for " << years << " year(s) (" << simulation_days << " days)...\n";

        for (int day = 1; day <= simulation_days; ++day) {
            // Assign adjusters to repair queue machines
            assignAdjusters();

            // Update machines for running, failure, repairs
            updateMachines(day);

            // Update adjusters work
            updateAdjusters(day);

            // Track repair queue size and max queue length
            if ((int)repair_queue.size() > max_queue_length) {
                max_queue_length = (int)repair_queue.size();
            }

            timeline.emplace_back(day, "Queue length: " + to_string(repair_queue.size()));
        }

        displayResults();
    }


    void assignAdjusters() {
        int qsize = (int)repair_queue.size();
        for (int i = 0; i < qsize; ++i) {
            if (repair_queue.empty()) break;
            MachineInstance* m = repair_queue.front();
            repair_queue.pop();

            bool assigned = false;

            for (size_t g = 0; g < adjusters.size() && !assigned; ++g) {
                // Check if adjuster group can work on this machine type
                if (!canAdjusterServiceMachine((int)g, machine_types[m->group_index].name)) continue;

                for (auto& adj : adjusters[g]) {
                    if (!adj.busy) {
                        // Assign
                        adj.busy = true;
                        adj.days_worked = 0;
                        adj.required_days = machine_types[m->group_index].repair_time;
                        adj.current_machine = m;
                        adj.total_busy_days++;

                        m->working = false;
                        m->repair_days = 1; // start counting repair days

                        // Log event
                        timeline.emplace_back(0, "Assign adjuster "
                            + to_string(adj.id_in_group + 1) + " of group " + adjuster_groups[g].id
                            + " to repair machine " + machine_types[m->group_index].name + " #" + to_string(m->id_in_group + 1));

                        assigned = true;
                        break;
                    }
                }
            }
            if (!assigned) {
                repair_queue.push(m); // enqueue back because no adjuster is free for now
            }
        }
    }

    void updateMachines(int current_day) {
        for (size_t g = 0; g < machines.size(); ++g) {
            for (auto& m : machines[g]) {
                if (m.working) {
                    m.running_days++;
                    if (m.running_days >= m.failure_day) {
                        // Machine fails now
                        m.working = false;
                        timeline.emplace_back(current_day, "Machine " + machine_types[g].name + " #" + to_string(m.id_in_group + 1) + " failed");
                        m.running_days = 0;
                        m.repair_days = 0;
                        // Randomize next failure day for after next repair cycle:
                        m.failure_day = randomizedFailureDay(machine_types[g].MTTF_days);

                        repair_queue.push(&m);
                    }
                }
                else {
                    if (m.repair_days > 0) {
                        // Under repair, increase repair days count
                        // actual incremented when adjuster works on it
                    }
                    // else waiting for assignment
                }
            }
        }
    }

    void updateAdjusters(int current_day) {
        for (size_t g = 0; g < adjusters.size(); ++g) {
            for (auto& adj : adjusters[g]) {
                if (adj.busy) {
                    adj.days_worked++;
                    adj.total_busy_days++;
                    if (adj.days_worked >= adj.required_days) {
                        // Repair done
                        timeline.emplace_back(current_day, "Adjuster " + to_string(adj.id_in_group + 1) + " of group "
                            + adjuster_groups[g].id + " finished repair on machine "
                            + machine_types[adj.current_machine->group_index].name + " #"
                            + to_string(adj.current_machine->id_in_group + 1));

                        adj.busy = false;
                        adj.days_worked = 0;
                        adj.required_days = 0;

                        // Mark machine as repaired
                        adj.current_machine->working = true;
                        adj.current_machine->repair_days = 0;
                        adj.current_machine->running_days = 0;

                        adj.current_machine = nullptr;
                    }
                }
            }
        }
    }

    void displayResults() {
        cout << "\n=== Simulation Results ===\n";

        cout << "\nMachine Utilization:\n";
        cout << left << setw(25) << "Machine Type" << setw(15) << "Quantity" << setw(20) << "Estimated Uptime(%)" << "\n";
        cout << string(60, '-') << "\n";

        long long total_machine_days = 0;
        long long total_machine_working_days = 0;

        for (size_t g = 0; g < machine_types.size(); ++g) {
            int q = machine_types[g].quantity;
            total_machine_days += (long long)q * simulation_days;

            // Sum total working days over all machines
            long long working_days = 0;
            for (const auto& m : machines[g]) {
                working_days += m.working ? m.running_days : 0;
            }
            total_machine_working_days += working_days;

            double uptime = total_machine_days > 0 ? 100.0 * working_days / ((long long)q * simulation_days) : 0.0;

            cout << left << setw(25) << machine_types[g].name << setw(15) << q << setw(20) << fixed << setprecision(2) << uptime << "\n";
        }

        double overall_machine_util = total_machine_days > 0 ? 100.0 * total_machine_working_days / total_machine_days : 0;
        cout << "\nOverall machine utilization: " << fixed << setprecision(2) << overall_machine_util << "%\n";

        cout << "\nAdjuster Utilization:\n";
        cout << left << setw(15) << "Adjuster ID" << setw(15) << "Count" << setw(25) << "Estimated Utilization(%)" << "\n";
        cout << string(60, '-') << "\n";

        long long total_adjuster_days = 0;
        long long total_adjuster_busy_days = 0;

        for (size_t g = 0; g < adjuster_groups.size(); ++g) {
            int c = adjuster_groups[g].count;
            total_adjuster_days += (long long)c * simulation_days;

            long long busy_days = 0;
            for (const auto& adj : adjusters[g]) {
                busy_days += adj.total_busy_days;
            }
            total_adjuster_busy_days += busy_days;

            double util = total_adjuster_days > 0 ? 100.0 * busy_days / ((long long)c * simulation_days) : 0;

            cout << left << setw(15) << adjuster_groups[g].id << setw(15) << c << setw(25) << fixed << setprecision(2) << util << "\n";
        }
        double overall_adj_util = total_adjuster_days > 0 ? 100.0 * total_adjuster_busy_days / total_adjuster_days : 0;
        cout << "\nOverall adjuster utilization: " << fixed << setprecision(2) << overall_adj_util << "%\n";

        cout << "\nMax repair queue length during simulation: " << max_queue_length << "\n";

        // Show timeline summary (last 10 events)
        cout << "\nRecent Simulation Events (last 10):\n";
        int start = (int)timeline.size() > 10 ? (int)timeline.size() - 10 : 0;
        for (size_t i = start; i < timeline.size(); ++i) {
            cout << "Day " << timeline[i].day << ": " << timeline[i].description << "\n";
        }

        // Detail viewing menu
        while (true) {
            cout << "\nView Details:\n1. Machine Types\n2. Adjuster Groups\n3. Exit\n";
            int choice = getIntInput("Select option: ", 1, 3);
            if (choice == 3) break;
            if (choice == 1) showMachineDetails();
            else if (choice == 2) showAdjusterDetails();
        }
    }

    void showMachineDetails() {
        if (machine_types.empty()) {
            cout << "No machine types.\n";
            return;
        }
        cout << "Machine Types:\n";
        for (size_t i = 0; i < machine_types.size(); ++i) {
            cout << i + 1 << ". " << machine_types[i].name << "\n";
        }
        int sel = getIntInput("Select machine type: ", 1, (int)machine_types.size());
        size_t idx = sel - 1;

        cout << "\nDetails of machine: " << machine_types[idx].name << "\n";
        cout << "MTTF (days): " << machine_types[idx].MTTF_days << "\n";
        cout << "Repair time (days): " << machine_types[idx].repair_time << "\n";
        cout << "Quantity: " << machine_types[idx].quantity << "\n";

        if (machines.size() <= idx) {
            cout << "No instances available.\n";
            return;
        }

        int working_count = 0, broken_count = 0;
        for (const auto& m : machines[idx]) {
            if (m.working)
                working_count++;
            else
                broken_count++;
        }
        cout << "Currently working: " << working_count << "\n";
        cout << "Currently broken/repairing: " << broken_count << "\n";
    }

    void showAdjusterDetails() {
        if (adjuster_groups.empty()) {
            cout << "No adjuster groups.\n";
            return;
        }
        cout << "Adjuster Groups:\n";
        for (size_t i = 0; i < adjuster_groups.size(); ++i) {
            cout << i + 1 << ". " << adjuster_groups[i].id << "\n";
        }
        int sel = getIntInput("Select adjuster group: ", 1, (int)adjuster_groups.size());
        size_t idx = sel - 1;

        cout << "\nAdjuster Group: " << adjuster_groups[idx].id << "\n";
        cout << "Count: " << adjuster_groups[idx].count << "\n";
        cout << "Services machine types:\n";
        for (const string& m : adjuster_groups[idx].capable_machines) {
            cout << "  - " << m << "\n";
        }

        if (adjusters.size() <= idx) {
            cout << "No adjuster instances available.\n";
            return;
        }

        int busy_count = 0, idle_count = 0;
        for (const auto& a : adjusters[idx]) {
            if (a.busy)
                busy_count++;
            else
                idle_count++;
        }
        cout << "Currently busy: " << busy_count << "\n";
        cout << "Currently idle: " << idle_count << "\n";
    }

    void mainMenu() {
        while (true) {
            cout << "\n=== Factory Maintenance Optimization Simulator ===\n";
            cout << "1. Add Machine Type\n";
            cout << "2. Add Adjuster Group\n";
            cout << "3. Run Simulation\n";
            cout << "4. Exit\n";

            int choice = getIntInput("Select option: ", 1, 4);
            switch (choice) {
            case 1: addMachineType(); break;
            case 2: addAdjusterGroup(); break;
            case 3: runSimulation(); break;
            case 4: cout << "Goodbye!\n"; return;
            }
        }
    }
};


// ------------------- Main -------------------

int main() {
    FMSSimulator sim;
    sim.mainMenu();
    return 0;
}
    