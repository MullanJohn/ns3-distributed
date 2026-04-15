import itertools
import os
import subprocess
from pathlib import Path
from concurrent.futures import ProcessPoolExecutor, as_completed

NS3_ROOT = Path(__file__).resolve().parent.parent.parent.parent.parent
EXECUTABLE = "continuum-wireless-evaluation-mu"

BASE_DIR = Path(__file__).resolve().parent / "results_06_bsrp"

N_CLIENTS = [1, 2]
FRAME_RATE = 30.0
BSRP_VALUES = [("off", "false"), ("on", "true")]
SEEDS = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
RUNS = [1]
SIM_TIME = 40.0
JOBS = 10

MEAN_FRAME_SIZE = 60000
COMPUTE_DEMAND = 8.7e9
OUTPUT_SIZE = 1000

PARAMS = {
    "scheme": "LL-NS",
    "computeDemand": COMPUTE_DEMAND,
    "maxActiveTasks": 4,
    "meanFrameSize": MEAN_FRAME_SIZE,
    "outputSize": OUTPUT_SIZE,
    "inputSizeStddev": 0.10 * MEAN_FRAME_SIZE,
    "computeDemandStddev": 0.10 * COMPUTE_DEMAND,
    "outputSizeStddev": 0.30 * OUTPUT_SIZE,
    "backboneRate": "1Gbps",
    "backboneDelay": "2us",
    "discRadius": 5.0,
}


def results_dir_for(bsrp_label):
    return BASE_DIR / f"fps{int(FRAME_RATE)}_{bsrp_label}"


def run_one(params):
    nClients, bsrp_label, bsrp_value, seed, runNumber = params
    resultsDir = results_dir_for(bsrp_label)
    args = [
        f"--nClients={nClients}",
        f"--seed={seed}",
        f"--runNumber={runNumber}",
        f"--simTime={SIM_TIME}",
        f"--frameRate={FRAME_RATE}",
        f"--enableBsrp={bsrp_value}",
        f"--outputDir={resultsDir}",
        "--deadlineBudgetMs=33.0",
        "--commBudgetMs=2.0",
    ]
    for key, value in PARAMS.items():
        args.append(f"--{key}={value}")

    cmd = [str(NS3_ROOT / "ns3"), "run", f"{EXECUTABLE} {' '.join(args)}"]
    env = dict(os.environ)
    env.pop("VIRTUAL_ENV", None)
    env["PATH"] = ":".join(
        p for p in env.get("PATH", "").split(":") if ".venv" not in p
    )
    result = subprocess.run(cmd, cwd=str(NS3_ROOT), capture_output=True, text=True, timeout=2400, env=env)
    if result.returncode != 0:
        print(f"STDERR: {result.stderr[-500:]}", flush=True)
    return params, result.returncode


def main():
    for label, _ in BSRP_VALUES:
        results_dir_for(label).mkdir(parents=True, exist_ok=True)

    combos = [
        (c, label, bsrp, s, r)
        for c, (label, bsrp), s, r in itertools.product(
            N_CLIENTS, BSRP_VALUES, SEEDS, RUNS
        )
    ]
    total = len(combos)
    print(f"Running {total} simulations with {JOBS} workers")

    completed = 0
    with ProcessPoolExecutor(max_workers=JOBS) as pool:
        futures = {pool.submit(run_one, p): p for p in combos}
        for future in as_completed(futures):
            params, rc = future.result()
            completed += 1
            nClients, label, _, seed, run = params
            status = "OK" if rc == 0 else "FAIL"
            print(f"[{completed}/{total}] {status} bsrp={label} c={nClients} s={seed} r={run}")


if __name__ == "__main__":
    main()
