#!/usr/bin/env python3
"""Real Linux integration test. Refuses to replace an existing /roudi."""
import array
import fcntl
import mmap
import os
from pathlib import Path
import re
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import time

BIN = Path(sys.argv[1] if len(sys.argv) > 1 else "build").resolve()
processes = []
files = []


def start(name, label, *args):
    log = open(logdir / (label + ".log"), "w+")
    files.append(log)
    p = subprocess.Popen([str(BIN / name), *args], stdout=log, stderr=subprocess.STDOUT)
    processes.append(p)
    return p, log


def contents(log):
    log.flush()
    return Path(log.name).read_text()


def wait_for(predicate, description, timeout=6):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        if predicate():
            return
        time.sleep(.03)
    raise AssertionError("timeout: " + description)


def stop(p, crash=False):
    p.send_signal(signal.SIGKILL if crash else signal.SIGTERM)
    result = p.wait(timeout=4)
    assert result == (-signal.SIGKILL if crash else 0), (p.pid, result)


def quotes(log, pid):
    return re.findall(rf"QUOTE pid={pid} seq=(\d+) price=(\d+) quantity=(\d+)", contents(log))


def assigned_worker(log, pid, symbol):
    matches = re.findall(rf"SUBSCRIBE pid={pid} symbol={symbol} instrument=\S+ worker=(\d+)", contents(log))
    assert matches and len(set(matches)) == 1, (pid, symbol, matches)
    return int(matches[0])


def mappings(pid):
    return sum("/memfd:quotes" in line for line in Path(f"/proc/{pid}/maps").read_text().splitlines())


if Path("/dev/shm/roudi").exists():
    sys.exit("Refusing to run: /roudi already exists; use an isolated IPC namespace or stop its owner.")

with tempfile.TemporaryDirectory(prefix="roudi-test-") as tmp:
    logdir = Path(tmp)
    try:
        p1, l1 = start("producer", "producer1", "--symbols", "AAPL:AAPL_C1,AAPL:AAPL_C2,MSFT:MSFT_C1")
        p2, l2 = start("producer", "producer2", "--symbols", "TSLA:TSLA_C1")
        wait_for(lambda: "PRODUCER" in contents(l1) and "PRODUCER" in contents(l2), "producers start")
        time.sleep(.3)
        assert not Path("/dev/shm/roudi").exists(), "producer created registry"
        c1, cl1 = start("consumer", "consumer1")
        wait_for(lambda: len(quotes(cl1, p1.pid)) > 5 and len(quotes(cl1, p2.pid)) > 5, "both early producers deliver")
        c_extra, extra_log = start("consumer", "extra")
        assert c_extra.wait(timeout=3) == 1
        assert "another consumer" in contents(extra_log)
        # Simulate a registrant dying halfway through a slot transaction.
        # Slot 63 is unused here. Leave a partial record with committed=0.
        marker = logdir / "lock-held"
        writer_code = """
import fcntl, mmap, pathlib, struct, sys, time
with open('/dev/shm/roudi', 'r+b', buffering=0) as f:
    fcntl.flock(f, fcntl.LOCK_EX)
    m = mmap.mmap(f.fileno(), 0)
    struct.pack_into('=QQ', m, 24 + 63 * 152, 0, 999)
    pathlib.Path(sys.argv[1]).write_text('ready')
    time.sleep(30)
"""
        writer = subprocess.Popen([sys.executable, "-c", writer_code, str(marker)])
        processes.append(writer)
        wait_for(marker.exists, "registry writer holds lock")
        count = len(quotes(cl1, p1.pid))
        wait_for(lambda: len(quotes(cl1, p1.pid)) > count + 3, "data path continues while registry is locked")
        stop(writer, crash=True)
        p3, l3 = start("producer", "producer3", "--symbols", "AAPL:AAPL_C1,NVDA:NVDA_C1")
        wait_for(lambda: len(quotes(cl1, p3.pid)) > 5, "late producer delivers")
        print("PASS producer-first startup, 3 producers, singleton consumer, killed registry writer")

        specs = ["AMD", "AAPL:AAPL_C3,NVDA:NVDA_C2", "GOOG", "META", "IBM", "ORCL"]
        extra_producers = [start("producer", f"balanced-{i}", "--symbols", spec)[0] for i, spec in enumerate(specs)]
        all_producers = [p1, p2, p3] + extra_producers
        wait_for(lambda: all(len(quotes(cl1, p.pid)) > 5 for p in all_producers),
                 "nine producers deliver")
        owners = {}
        for symbol, worker in re.findall(r"SUBSCRIBE pid=\d+ symbol=(\S+) instrument=\S+ worker=(\d+)", contents(cl1)):
            assert symbol not in owners or owners[symbol] == int(worker), "underlying assigned to multiple workers"
            owners[symbol] = int(worker)
        assert len(owners) == 9
        assert [list(owners.values()).count(i) for i in range(3)] == [3, 3, 3], owners
        assert assigned_worker(cl1, p1.pid, "AAPL") == assigned_worker(cl1, p3.pid, "AAPL") == assigned_worker(cl1, extra_producers[1].pid, "AAPL")
        assert assigned_worker(cl1, p1.pid, "AAPL") != assigned_worker(cl1, p1.pid, "MSFT")
        assert len(list(Path(f"/proc/{c1.pid}/task").iterdir())) == 4
        assert mappings(c1.pid) == 9, "expected exactly one mapping per producer"
        for p in extra_producers:
            stop(p)
        wait_for(lambda: all(f"DETACH pid={p.pid} " in contents(cl1) for p in extra_producers),
                 "extra producers detach")
        wait_for(lambda: mappings(c1.pid) == 3, "extra producer mappings released")
        print("PASS nine underlyings balanced 3/3/3; AAPL from producers 1/3/5 shares one reader")
        print("PASS multiple contracts per underlying, mixed-symbol producer, one mapping per producer")

        # Twelve live but unresponsive registry endpoints keep discovery in FD
        # handshakes for ~1.2 seconds. Existing quotes must keep flowing meanwhile.
        slow_path = f"/tmp/roudi-ipc-{os.getuid()}/test-slow-{os.getpid()}.sock"
        slow_ids = set(range(0xDEAD0000, 0xDEAD000C))
        with socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET) as slow:
            slow.bind(slow_path)
            slow.listen(32)
            slow.settimeout(3)
            try:
                stat = Path(f"/proc/{os.getpid()}/stat").read_text()
                ticks = int(stat[stat.rfind(")") + 2:].split()[19])
                with open("/dev/shm/roudi", "r+b", buffering=0) as f:
                    fcntl.flock(f, fcntl.LOCK_EX)
                    with mmap.mmap(f.fileno(), 0) as reg:
                        for i, identity in enumerate(sorted(slow_ids), start=40):
                            struct.pack_into("=QQQQiI108sI", reg, 24 + i * 152,
                                             1, identity, ticks, time.monotonic_ns(),
                                             os.getpid(), 0, slow_path.encode(), 0)
                client, _ = slow.accept()
                with client:
                    count = len(quotes(cl1, p1.pid))
                    wait_for(lambda: len(quotes(cl1, p1.pid)) > count + 5,
                             "reader continues during stalled discovery", timeout=.6)
                assert len(list(Path(f"/proc/{c1.pid}/task").iterdir())) == 4
            finally:
                with open("/dev/shm/roudi", "r+b", buffering=0) as f:
                    fcntl.flock(f, fcntl.LOCK_EX)
                    with mmap.mmap(f.fileno(), 0) as reg:
                        for i in range(64):
                            offset = 24 + i * 152
                            if struct.unpack_from("=Q", reg, offset + 8)[0] in slow_ids:
                                struct.pack_into("=Q", reg, offset, 0)
                Path(slow_path).unlink(missing_ok=True)
        print("PASS reader threads deliver quotes during stalled discovery")

        # Validate packet framing, versions, generation and actual SCM_RIGHTS.
        identity = int(re.search(r"identity=(\d+)", contents(l1)).group(1))
        path = f"/tmp/roudi-ipc-{os.getuid()}/p-{p1.pid}-{identity}.sock"
        fmt = "=IIIIQQII"
        for version, ident, suffix, good in [(2, identity, b"", True), (1, identity, b"", False), (2, identity ^ 1, b"", False), (2, identity, b"extra", False)]:
            with socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET) as sock:
                sock.settimeout(2)
                sock.connect(path)
                sock.send(struct.pack(fmt, 0x524f5544, version, 40, 1, ident, 12345, 0, 0) + suffix)
                data, ancillary, flags, _ = sock.recvmsg(40, socket.CMSG_SPACE(32))
                response = struct.unpack(fmt, data)
                received = []
                for level, kind, payload in ancillary:
                    assert level == socket.SOL_SOCKET and kind == socket.SCM_RIGHTS
                    fds = array.array("i")
                    fds.frombytes(payload)
                    received.extend(fds)
                try:
                    assert not flags and response[5] == 12345
                    assert (response[6] == 0) == good
                    assert len(received) == (1 if good else 0)
                    if good:
                        assert os.fstat(received[0]).st_size == 67232
                        with mmap.mmap(received[0], 0, access=mmap.ACCESS_READ) as region:
                            assert struct.unpack_from("=I", region, 24)[0] == 3
                            assert region[32:64].split(b"\0")[0] == b"AAPL"
                finally:
                    for fd in received:
                        os.close(fd)
        print("PASS FD transfer and rejection of wrong version, identity, packet size")

        # Pausing isn't death. A heartbeat timeout must not evict a live producer.
        p2.send_signal(signal.SIGSTOP)
        time.sleep(.5)
        assert f"DETACH pid={p2.pid}" not in contents(cl1)
        p2.send_signal(signal.SIGCONT)
        # Deliberate ring overrun must be reported and recovery must continue.
        c1.send_signal(signal.SIGSTOP)
        time.sleep(3.2)
        c1.send_signal(signal.SIGCONT)
        wait_for(lambda: "DROPPED" in contents(cl1), "overrun reported")
        print("PASS stopped producer remains attached; slow-consumer overrun detected")

        stop(p1, crash=True)
        wait_for(lambda: f"DETACH pid={p1.pid}" in contents(cl1), "crash detached")
        after = contents(cl1).split(f"DETACH pid={p1.pid}", 1)[1]
        assert f"QUOTE pid={p1.pid} " not in after
        stop(p2)
        wait_for(lambda: f"DETACH pid={p2.pid}" in contents(cl1), "graceful exit detached")

        def stale_slots_removed():
            with open("/dev/shm/roudi", "rb") as registry:
                data = registry.read()
            for i in range(64):
                offset = 24 + i * 152
                committed = struct.unpack_from("=Q", data, offset)[0]
                pid = struct.unpack_from("=i", data, offset + 32)[0]
                if committed and pid in (p1.pid, p2.pid):
                    return False
            return True
        wait_for(stale_slots_removed, "dead registry slots reclaimed")
        wait_for(lambda: mappings(c1.pid) == 1, "shared mappings released after all subscriptions detach")
        print("PASS SIGKILL and graceful exit detach mappings and reclaim slots")

        counts = [0, 0, 0]
        for symbol in ("AAPL", "NVDA"):
            counts[assigned_worker(cl1, p3.pid, symbol)] += 1
        replacement, _ = start("producer", "replacement", "--symbols", "AAPL:AAPL_C4,NFLX:NFLX_C1")
        wait_for(lambda: len(quotes(cl1, replacement.pid)) > 5, "replacement producer delivers")
        assert assigned_worker(cl1, replacement.pid, "AAPL") == assigned_worker(cl1, p3.pid, "AAPL")
        assert assigned_worker(cl1, replacement.pid, "NFLX") == counts.index(min(counts))
        stop(replacement)
        wait_for(lambda: f"DETACH pid={replacement.pid} " in contents(cl1), "replacement detached")
        print("PASS existing underlying retains affinity; new underlying uses least-loaded reader")

        stop(c1, crash=True)
        c2, cl2 = start("consumer", "consumer2")
        wait_for(lambda: len(quotes(cl2, p3.pid)) > 5, "survivor re-registers after consumer crash")
        stop(c2)
        assert not Path("/dev/shm/roudi").exists()
        c3, cl3 = start("consumer", "consumer3")
        wait_for(lambda: len(quotes(cl3, p3.pid)) > 5, "survivor re-registers after graceful restart")
        stop(p3)
        wait_for(lambda: f"DETACH pid={p3.pid}" in contents(cl3), "last producer detached")
        stop(c3)
        for log in (cl1, cl2, cl3):
            data = contents(log)
            assert "consumer:" not in data
            last_sequence = {}
            owner = {}
            detached = set()
            unsubscribed = set()
            for line in data.splitlines():
                match = re.fullmatch(r"SUBSCRIBE pid=(\d+) symbol=(\S+) instrument=(\S+) worker=(\d+)", line)
                if match:
                    pid, symbol, instrument, worker = match.groups()
                    key = (int(pid), symbol, instrument)
                    assert key not in owner, "duplicate subscription"
                    owner[key] = int(worker)
                match = re.fullmatch(r"UNSUBSCRIBE pid=(\d+) symbol=(\S+) instrument=(\S+) worker=(\d+)", line)
                if match:
                    pid, symbol, instrument, worker = match.groups()
                    key = (int(pid), symbol, instrument)
                    assert owner[key] == int(worker)
                    unsubscribed.add(key)
                match = re.fullmatch(r"DETACH pid=(\d+) identity=\d+", line)
                if match:
                    pid = int(match.group(1))
                    assert all(key in unsubscribed for key in owner if key[0] == pid)
                    detached.add(pid)
                match = re.fullmatch(r"QUOTE pid=(\d+) seq=(\d+) price=\d+ quantity=\d+ symbol=(\S+) instrument=(\S+) worker=(\d+)", line)
                if match:
                    pid, seq, symbol, instrument, worker = match.groups()
                    pid, seq, worker = int(pid), int(seq), int(worker)
                    key = (pid, symbol, instrument)
                    assert pid not in detached, "quote after detach"
                    assert key not in unsubscribed, "quote after unsubscribe"
                    assert owner[key] == worker, "stream read by wrong worker"
                    assert seq > last_sequence.get(key, 0), "duplicate or out-of-order quote"
                    last_sequence[key] = seq
            assert set(last_sequence) == set(owner), "an instrument never delivered quotes"
            for seq, price, quantity in re.findall(r"seq=(\d+) price=(\d+) quantity=(\d+)", data):
                assert int(price) == int(seq) and int(quantity) == 3 * int(seq), "torn quote"
        print("PASS consumer restart, quote integrity, single-reader ownership and ordering")
        print("ALL SMOKE TESTS PASSED")
    except BaseException:
        for log in files:
            print("\nLOG", log.name, "\n", contents(log)[-5000:])
        raise
    finally:
        for p in processes:
            if p.poll() is None:
                p.send_signal(signal.SIGCONT)
                p.terminate()
                try:
                    p.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    p.kill()
                    p.wait()
        for log in files:
            log.close()
        # Remove socket leftovers only for producer PIDs created by this test.
        runtime = Path(f"/tmp/roudi-ipc-{os.getuid()}")
        for p in processes:
            for sock in runtime.glob(f"p-{p.pid}-*.sock"):
                sock.unlink(missing_ok=True)
