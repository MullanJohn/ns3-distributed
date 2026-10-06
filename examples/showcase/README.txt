1. Build

From the ns-3 root (/home/ahzahran/Development/dcs-simulation/ns-3-dev):

cmake --build cmake-cache -j"$(nproc)" --target continuum-showcase-example

(./ns3 is broken under this environment's Python, so use the cmake-cache build directly — same as everywhere else this session.)

2. Quick single run (sanity check)

./build/contrib/continuum/examples/ns3.44-continuum-showcase-example-default \
  --scheduler=networkaware --tier2DelayMs=4 --simTim
  --outputDir=contrib/continuum/examples/showcase/results

Prints a summary to stdout and writes results/networkaware/tier2Delay_4ms_seed_1_{client,orchestrator}.csv. Useful flags:
--scheduler={leastloaded,networkaware,tiereddelay}, deadlineBudgetMs, --RngRun (repetition seed) — run--PrintHelp for the full list.

3. Full sweep

contrib/continuum/examples/showcase/run_sweep.sh

Sweeps all 3 schedulers × tier2Delay ∈ {2,4,6,8,10} ms × 10 seeds (30 runs by default) into
contrib/continuum/examples/showcase/results/<schedul e.g.:

RUNS=20 TIER2_DELAYS="2 6 10" contrib/continuum/exam

Anything after the script name is forwarded to the b --simTime=10.

Anything after the script name is forwarded to the binary, e.g. ... run_sweep.sh --simTime=10.

4. Generate the figures

Needs pandas + matplotlib (not preinstalled here — I installed them via pip install --break-system-packages --user pandas matplotlib earlier for verification; use whatever Python env you prefer):

python3 contrib/continuum/examples/showcase/plot_results.py \
  --results-dir contrib/continuum/examples/showcase/results \
  --fontsize 11
