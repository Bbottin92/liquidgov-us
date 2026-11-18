# Distributed consensus — not this game

Not a BUILD. Does not reopen presence, sibling deploy, or LGP1.

The Commons occupancy is **one process, RAM, 96 cap, ~2 Hz**
(`presence.md`, `presence_accounting.md`). The ledger is **one SQLite
file, WAL, one writer** (`sibling.md`). There is no second replica to
agree with.

## What people mean

| Family | Agrees on | Cost |
|---|---|---|
| Paxos / Raft / VR | A **log** of operations, one leader | 3+ boxes, quorum, elections |
| Zab (ZooKeeper) | Ordered broadcasts | Same |
| PBFT / HotStuff | Log, **Byzantine** voters | 3f+1, crypto, not 512 MB |
| Nakamoto | Probable chain tip | Hash power, minutes, not 2 Hz |
| CRDTs | Merge without electing | No single order; presence ids would split |

Raft is “who may append to the log.” Presence is “who is in the Square.”
Those are different questions. A crashed tab is an idle-reap problem,
not an election.

## Why it misses here

- **One droplet.** Civic + game already share 512 MB / 1 vCPU. A 3-node
  Raft for 96 sprites is three times the blast radius Genesis must not
  take.
- **SQLite WAL is already single-writer.** A second daemon opening the
  same file is a lock, not a cluster. Do not rsync the live DB
  (`sibling.md`).
- **LGP1 is fail-closed, not Raft.** Civic’s store is one 0600 file,
  tmp+fsync+rename. It does not vote. The game must not grow LGP1 tags
  to “consensus presence.”
- **2 Hz occupancy is not a replicated log.** Clients already lie until
  cookie glue; a quorum of liars is worse.
- **CRDT maps** (`gset` of occupant ids) converge by union: leave never
  wins. Presence **must** forget.

## If there were ever two boxes

Then you still would not Raft the **sprites**. You would:

1. Keep presence on **one** game daemon (the leader is the process).
2. Fail over with systemd restart; clients re-join (RAM is allowed to
   die — that is the spec).
3. SQLite stays on that box; civic stays on its own unit.

A multi-region Commons is a different product. Not V1, not this week.

## Reviewer fail

- etcd / consul / a Raft crate in `commons-daemon/`
- Presence rows in SQLite “so we can replicate”
- Blockchain occupancy
- Two daemons both binding 8765 and “eventually consistent” walkers

Open work is still body cap, idle reap, id-index. No deploy.
