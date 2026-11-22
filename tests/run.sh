#!/bin/sh
# Build and run the host tests: load-chart logic (packages/load_chart.yaml), night hours
# (packages/settings.h) and the status bar (packages/status.h).
set -e
cd "$(dirname "$0")"
python3 extract.py ../packages/load_chart.yaml lambdas.inc
c++ -std=c++17 -Wall -Wextra -O1 -o chart_logic_test chart_logic_test.cpp
./chart_logic_test
c++ -std=c++17 -Wall -Wextra -O1 -o settings_test settings_test.cpp
./settings_test
c++ -std=c++17 -Wall -Wextra -O1 -o status_test status_test.cpp
./status_test
python3 check_device_builder.py
