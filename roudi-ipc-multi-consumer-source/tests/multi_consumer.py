#!/usr/bin/env python3
"""Fan-out and owner lifecycle integration test; uses only its own processes."""
import fcntl
import os
from pathlib import Path
import re
import signal
import struct
import subprocess
import sys
import tempfile
import time

BIN = Path(sys.argv[1] if len(sys.argv) > 1 else "build").resolve()
processes = []
logs = []


def launch(executable, label, *args):
    log = open(logdir / f"{label}.log", "w+")
    logs.append(log)
    process = subprocess.Popen([str(BIN / executable), *args], stdout=log, stderr=subprocess.STDOUT)
    processes.append(process)
    return process, log


def text(log):
    return Path(log.name).read_text()


def wait_for(predicate, reason, timeout=7):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if predicate():
            return
        time.sleep(.03)
    raise AssertionError("timeout: " + reason)


def stop(process, crash=False):
    process.send_signal(signal.SIGKILL if crash else signal.SIGTERM)
    assert process.wait(timeout=4) == (-signal.SIGKILL if crash else 0)


def quote_count(log, pid):
    return text(log).count(f"QUOTE pid={pid} ")


def has_all(log, producers):
    return all(quote_count(log, producer.pid) > 5 for producer in producers)


def advances(log, producer, count=8):
    before = quote_count(log, producer.pid)
    wait_for(lambda: quote_count(log, producer.pid) > before + count, "live quote progress")


def epoch(log):
    matches = re.findall(r"READY mode=\w+ epoch=(\d+)", text(log))
    return int(matches[-1]) if matches else None


def committed_pid(pid):
    with open("/dev/shm/roudi", "rb") as registry:
        fcntl.flock(registry, fcntl.LOCK_SH)
        data = registry.read()
    return any(struct.unpack_from("=Q", data, 24 + i * 152)[0] == 1 and
               struct.unpack_from("=i", data, 24 + i * 152 + 32)[0] == pid
               for i in range(64))


def sequence_sets(log):
    result = {}
    last = {}
    routes = {}
    subscriptions = {}
    detached = set()
    for line in text(log).splitlines():
        if line.startswith("DETACH "):
            detached.add(int(re.search(r"pid=(\d+)", line).group(1)))
        match = re.fullmatch(r"SUBSCRIBE pid=(\d+) symbol=(\S+) instrument=(\S+) worker=(\d+)", line)
        if match:
            pid, symbol, instrument, worker = match.groups()
            key = (int(pid), symbol, instrument)
            assert key not in subscriptions, "duplicate attachment across registry epochs"
            subscriptions[key] = int(worker)
        match = re.fullmatch(r"QUOTE pid=(\d+) seq=(\d+) price=(\d+) quantity=(\d+) symbol=(\S+) instrument=(\S+) worker=(\d+)", line)
        if not match:
            continue
        pid, seq, price, quantity, symbol, instrument, worker = match.groups()
        pid, seq, worker = int(pid), int(seq), int(worker)
        assert pid not in detached, "quote after producer detach"
        assert int(price) == seq and int(quantity) == seq * 3
        key = (pid, symbol, instrument)
        assert seq > last.get(key, 0), "duplicate/replayed/out-of-order quote"
        assert subscriptions[key] == worker
        # These fixtures retain AAPL/MSFT via the long-lived producer, so their
        # routes must never change inside one consumer, including owner restarts.
        if symbol in ("AAPL", "MSFT"):
            assert symbol not in routes or routes[symbol] == worker
            routes[symbol] = worker
        last[key] = seq
        result.setdefault(key, set()).add(seq)
    assert result
    return result


if Path("/dev/shm/roudi").exists():
    sys.exit("Refusing test: /roudi already exists; use an isolated IPC namespace.")

with tempfile.TemporaryDirectory(prefix="roudi-fanout-") as temporary:
    logdir = Path(temporary)
    try:
        # Mode selection is mandatory; an accidental invocation never creates.
        bad, _ = launch("consumer", "missing-mode")
        assert bad.wait(timeout=3) == 1
        r1, l1 = launch("consumer", "reader1", "--reader")
        r2, l2 = launch("consumer", "reader2", "--reader")
        p1, _ = launch("producer", "producer1", "--symbols", "AAPL:AAPL_C1,MSFT:MSFT_C1")
        p2, _ = launch("producer", "producer2", "--symbols", "AAPL:AAPL_C2,TSLA:TSLA_C1")
        wait_for(lambda: "START mode=reader" in text(l1) and "START mode=reader" in text(l2), "readers start without owner")
        time.sleep(.3)
        assert not Path("/dev/shm/roudi").exists()
        o1, ol1 = launch("consumer", "owner1", "--owner")
        wait_for(lambda: all(has_all(log, [p1, p2]) for log in (ol1, l1, l2)), "owner and two readers fan out")
        initial_inode = os.stat("/dev/shm/roudi").st_ino
        extra, extra_log = launch("consumer", "second-owner", "--owner")
        assert extra.wait(timeout=3) == 1
        assert "registry owner" in text(extra_log)
        assert os.stat("/dev/shm/roudi").st_ino == initial_inode
        for consumer in (o1, r1, r2):
            mappings = [line for line in Path(f"/proc/{consumer.pid}/maps").read_text().splitlines()
                        if "/memfd:quotes" in line or "/dev/shm/roudi-quotes-" in line]
            assert len(mappings) == 2
            assert all(line.split()[1] == "r--s" for line in mappings), "consumer quote mapping must be read-only"
            assert len(list(Path(f"/proc/{consumer.pid}/task").iterdir())) == 4
        print("PASS readers/producers start before owner; singleton owner; fan-out; read-only quote mappings")

        # Stop only one consumer long enough to overwrite its independent cursor.
        r1.send_signal(signal.SIGSTOP)
        owner_before, reader_before = quote_count(ol1, p1.pid), quote_count(l2, p1.pid)
        time.sleep(3.2)
        assert quote_count(ol1, p1.pid) > owner_before + 20
        assert quote_count(l2, p1.pid) > reader_before + 20
        r1.send_signal(signal.SIGCONT)
        wait_for(lambda: "DROPPED" in text(l1), "slow reader reports its own overrun")
        assert "DROPPED" not in text(ol1) and "DROPPED" not in text(l2)
        print("PASS slow reader does not stall owner or other reader")

        stop(o1, crash=True)
        assert Path("/dev/shm/roudi").exists()
        advances(l1, p1)
        advances(l2, p1)
        stop(p2, crash=True)
        wait_for(lambda: all(f"DETACH pid={p2.pid} " in text(log) for log in (l1, l2)), "reader-local death detection")
        time.sleep(.2)
        assert committed_pid(p2.pid), "reader improperly reclaimed a dead registry slot"
        p3, _ = launch("producer", "producer3", "--symbols", "AAPL:AAPL_C3,NVDA:NVDA_C1")
        wait_for(lambda: all(has_all(log, [p3]) for log in (l1, l2)), "discover new producer in surviving registry")
        assert epoch(l1) == epoch(ol1)
        print("PASS owner crash preserves reading/discovery; readers detect deaths without mutating slots")

        o2, ol2 = launch("consumer", "owner2", "--owner")
        wait_for(lambda: has_all(ol2, [p1, p3]), "restarted owner discovers survivors")
        wait_for(lambda: epoch(l1) == epoch(ol2) == epoch(l2), "readers reopen replacement registry")
        assert epoch(ol2) != epoch(ol1)
        assert os.stat("/dev/shm/roudi").st_ino != initial_inode
        assert not committed_pid(p2.pid)
        p4, _ = launch("producer", "producer4", "--symbols", "AAPL:AAPL_C4,AMD:AMD_C1")
        wait_for(lambda: all(has_all(log, [p4]) for log in (ol2, l1, l2)), "late producer in new registry")
        for log in (l1, l2):
            assert len(re.findall(rf"ATTACH pid={p1.pid} identity=", text(log))) == 1
            assert len(re.findall(rf"ATTACH pid={p3.pid} identity=", text(log))) == 1
        print("PASS owner replacement/re-registration; reader cursors and mappings preserved")

        stop(o2)
        assert not Path("/dev/shm/roudi").exists()
        advances(l1, p1)
        advances(l2, p3)
        p5, _ = launch("producer", "producer5", "--symbols", "AAPL:AAPL_C5")
        time.sleep(.3)
        assert quote_count(l1, p5.pid) == 0
        assert not Path("/dev/shm/roudi").exists(), "reader created registry"
        o3, ol3 = launch("consumer", "owner3", "--owner")
        wait_for(lambda: all(has_all(log, [p1, p3, p4, p5]) for log in (ol3, l1, l2)), "recovery after graceful owner exit")
        print("PASS graceful owner exit leaves existing streams usable; absent registry retries")

        current_inode = os.stat("/dev/shm/roudi").st_ino
        stop(r1)
        assert os.stat("/dev/shm/roudi").st_ino == current_inode
        advances(ol3, p1)
        advances(l2, p1)
        stop(r2, crash=True)
        assert os.stat("/dev/shm/roudi").st_ino == current_inode
        r3, l3 = launch("consumer", "reader3", "--reader")
        wait_for(lambda: has_all(l3, [p1, p3, p4, p5]), "late reader obtains independent FDs")
        for producer in (p1, p3, p4, p5):
            stop(producer)
        wait_for(lambda: all(f"DETACH pid={p1.pid} " in text(log) for log in (ol3, l3)), "last producer detached")
        wait_for(lambda: not any(committed_pid(p.pid) for p in (p1, p3, p4, p5)), "owner reclaims stale slots")
        stop(r3)
        assert os.stat("/dev/shm/roudi").st_ino == current_inode
        stop(o3)
        assert not Path("/dev/shm/roudi").exists()
        print("PASS reader exit/crash does not unlink registry; owner alone cleans up")

        sets = [sequence_sets(log) for log in (ol1, l1, l2, ol2, ol3, l3)]
        key = (p1.pid, "AAPL", "AAPL_C1")
        assert len(sets[0][key] & sets[1][key] & sets[2][key]) >= 5, "not fan-out of same quotes"
        print("PASS same quotes delivered independently; symbol affinity and sequence integrity across owner restarts")
        print("ALL MULTI-CONSUMER TESTS PASSED")
    except BaseException:
        for log in logs:
            print("LOG", log.name, "\n", text(log)[-4000:])
        raise
    finally:
        for process in processes:
            if process.poll() is None:
                process.send_signal(signal.SIGCONT)
                process.terminate()
                try:
                    process.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
        for log in logs:
            log.close()
        runtime = Path(f"/tmp/roudi-ipc-{os.getuid()}")
        for process in processes:
            for path in runtime.glob(f"p-{process.pid}-*.sock"):
                path.unlink(missing_ok=True)
