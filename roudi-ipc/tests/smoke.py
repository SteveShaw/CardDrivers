#!/usr/bin/env python3
"""Real Linux integration test. Refuses to replace an existing /roudi."""
import array
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


def start(name, label):
    log = open(logdir / (label + ".log"), "w+")
    files.append(log)
    p = subprocess.Popen([str(BIN / name)], stdout=log, stderr=subprocess.STDOUT)
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


if Path("/dev/shm/roudi").exists():
    sys.exit("Refusing to run: /roudi already exists; use an isolated IPC namespace or stop its owner.")

with tempfile.TemporaryDirectory(prefix="roudi-test-") as tmp:
    logdir = Path(tmp)
    try:
        p1, l1 = start("producer", "producer1")
        p2, l2 = start("producer", "producer2")
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
        p3, l3 = start("producer", "producer3")
        wait_for(lambda: len(quotes(cl1, p3.pid)) > 5, "late producer delivers")
        print("PASS producer-first startup, 3 producers, singleton consumer, killed registry writer")

        # Validate packet framing, versions, generation and actual SCM_RIGHTS.
        identity = int(re.search(r"identity=(\d+)", contents(l1)).group(1))
        path = f"/tmp/roudi-ipc-{os.getuid()}/p-{p1.pid}-{identity}.sock"
        fmt = "=IIIIQQII"
        for version, ident, suffix, good in [(1, identity, b"", True), (2, identity, b"", False), (1, identity ^ 1, b"", False), (1, identity, b"extra", False)]:
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
                        assert os.fstat(received[0]).st_size == 4128
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
        print("PASS SIGKILL and graceful exit detach mappings and reclaim slots")

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
            for seq, price, quantity in re.findall(r"seq=(\d+) price=(\d+) quantity=(\d+)", data):
                assert int(price) == int(seq) and int(quantity) == 3 * int(seq), "torn quote"
        print("PASS consumer crash/graceful restart and quote integrity")
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
