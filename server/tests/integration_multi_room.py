#!/usr/bin/env python3
"""Black-box smoke test for room isolation and host/ready/start rules."""

from __future__ import annotations

import select
import socket
import sys
import time
from dataclasses import dataclass, field


@dataclass
class Client:
    sock: socket.socket
    buffer: bytes = b""
    lines: list[str] = field(default_factory=list)

    @classmethod
    def connect(cls, host: str, port: int, room: str, name: str) -> "Client":
        sock = socket.create_connection((host, port), timeout=3)
        client = cls(sock)
        client.send(f"JOIN|{room}|{name}")
        client.wait_for("JOINED|", 3)
        return client

    def send(self, line: str) -> None:
        self.sock.sendall((line + "\n").encode("utf-8"))

    def pump(self, timeout: float = 0.05) -> None:
        readable, _, _ = select.select([self.sock], [], [], timeout)
        if not readable:
            return
        data = self.sock.recv(65536)
        if not data:
            return
        self.buffer += data
        while b"\n" in self.buffer:
            raw, self.buffer = self.buffer.split(b"\n", 1)
            line = raw.decode("utf-8", errors="replace").strip()
            if line:
                self.lines.append(line)

    def wait_for(self, prefix: str, timeout: float) -> str:
        deadline = time.time() + timeout
        while time.time() < deadline:
            for line in self.lines:
                if line.startswith(prefix):
                    return line
            self.pump(min(0.05, deadline - time.time()))
        raise AssertionError(f"did not receive {prefix!r}; received {self.lines!r}")

    def has_prefix(self, prefix: str) -> bool:
        self.pump(0.05)
        return any(line.startswith(prefix) for line in self.lines)

    def close(self) -> None:
        try:
            self.sock.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        self.sock.close()


def main() -> int:
    host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 18888
    clients: list[Client] = []
    try:
        room_a = [Client.connect(host, port, "1111", f"A{i}") for i in range(1, 8)]
        room_b = [Client.connect(host, port, "2222", f"B{i}") for i in range(1, 8)]
        clients.extend(room_a + room_b)

        room_a[1].send("START")
        assert "只有房主" in room_a[1].wait_for("ERROR|HOST_ONLY|", 2)

        for client in room_a + room_b:
            client.send("READY|1")
        time.sleep(0.3)
        for client in room_a + room_b:
            client.pump(0.05)

        room_a[0].send("START")
        for client in room_a:
            client.wait_for("ROLE|", 3)
            client.wait_for("GAME_START|1111", 3)
        time.sleep(0.2)
        for client in room_b:
            assert not client.has_prefix("ROLE|"), "room B was affected when room A started"

        rejected = Client(socket.create_connection((host, port), timeout=3))
        clients.append(rejected)
        rejected.send("JOIN|1111|late")
        rejected.wait_for("JOIN_REJECTED|GAME_IN_PROGRESS|", 2)

        room_c = Client.connect(host, port, "3333", "C1")
        room_c_peer = Client.connect(host, port, "3333", "C2")
        clients.extend([room_c, room_c_peer])
        assert room_c.has_prefix("JOINED|3333|1|1|1")
        room_c.close()
        room_c_peer.wait_for("PLAYER_ID|1", 2)
        room_c_peer.wait_for("NOTICE|新房主|", 2)

        room_b[0].send("START")
        for client in room_b:
            client.wait_for("ROLE|", 3)
            client.wait_for("GAME_START|2222", 3)

        # Room A completes a timeout-driven night, votes, then reuses the same room.
        for client in room_a:
            client.wait_for("ACTION|VOTE|", 15)
            client.send("VOTE|1")
        for client in room_a:
            client.wait_for("RESULT|", 4)
            client.wait_for("GAME_RESET|", 6)
        room_a[0].send("READY|1")
        room_a[0].wait_for("READY_ACK|1|", 2)

        print("multi-room integration test passed")
        return 0
    finally:
        for client in clients:
            client.close()


if __name__ == "__main__":
    raise SystemExit(main())
