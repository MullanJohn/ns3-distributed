import os
import subprocess
from pathlib import Path
from concurrent.futures import ProcessPoolExecutor, as_completed

NS3_ROOT = Path(__file__).resolve().parent.parent.parent.parent.parent
EXECUTABLE = "distributed-dag-validation"

# DAG execution validation: runs the three shapes (single, chain, forkjoin)
# across 10 RNG seeds. The forkjoin shape is run on both 1 and 2 backends
# to demonstrate capacity-bound serialisation vs parallel dispatch.
# Workload and GPU parameters are kept identical to the p2p_homog sweep
# so that per-task compute time reproduces the existing analytical value
# (T_compute = 6.6246 ms at base OPP, 9 W static, RTX 5060 profile).
RESULTS_DIR = Path(__file__).resolve().parent / "results_dag_validation"

SCHEME = "LU-SG"
CONFIGS = [
    ("chain", 1),
    ("forkjoin", 1),
    ("forkjoin", 2),
]
SEEDS = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
RUNS = [1]
SIM_TIME = 3.0
JOBS = 3

PARAMS = {
    "computeDemand": 8.7e9,
    "inputSize": 60000,
    "outputSize": 1000,
    "deadlineBudgetMs": 1000.0,
    "maxActiveTasks": 16,
    "linkRate": "1Gbps",
    "linkDelay": "2us",
    "effectiveCapacitance": 1.94e-8,
}


def run_one(params):
    shape, nBackends, seed, runNumber = params
    args = [
        f"--shape={shape}",
        f"--nBackends={nBackends}",
        f"--scheme={SCHEME}",
        f"--seed={seed}",
        f"--runNumber={runNumber}",
        f"--simTime={SIM_TIME}",
        f"--outputDir={RESULTS_DIR}",
    ]
    for key, value in PARAMS.items():
        args.append(f"--{key}={value}")

    cmd = [str(NS3_ROOT / "ns3"), "run", f"{EXECUTABLE} {' '.join(args)}"]
    env = dict(os.environ)
    env.pop("VIRTUAL_ENV", None)
    env["PATH"] = ":".join(
        p for p in env.get("PATH", "").split(":") if ".venv" not in p
    )
    result = subprocess.run(cmd, cwd=str(NS3_ROOT), capture_output=True, text=True,
                            timeout=600, env=env)
    if result.returncode != 0:
        print(f"STDERR: {result.stderr[-500:]}", flush=True)
    return params, result.returncode


def main():
    RESULTS_DIR.mkdir(parents=True, exist_ok=True)

    combos = []
    for shape, nBackends in CONFIGS:
        for seed in SEEDS:
            for run in RUNS:
                combos.append((shape, nBackends, seed, run))
    total = len(combos)
    print(f"Running {total} simulations with {JOBS} workers")

    completed = 0
    with ProcessPoolExecutor(max_workers=JOBS) as pool:
        futures = {pool.submit(run_one, p): p for p in combos}
        for future in as_completed(futures):
            params, rc = future.result()
            completed += 1
            shape, nBackends, seed, run = params
            status = "OK" if rc == 0 else "FAIL"
            print(f"[{completed}/{total}] {status} {shape} b={nBackends} s={seed} r={run}")


if __name__ == "__main__":
    main()
