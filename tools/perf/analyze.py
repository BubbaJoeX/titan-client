"""Summarize stacks saved by sampler.py, leaving out samples where the game loop was waiting (in Present or the
frame limiter, asleep or spinning), so the percentages are shares of the work per frame.

usage: python analyze.py <stacks.pkl> [top N] [function]
With a function name, prints what that function spends its time in (its callees) instead."""
import collections
import pickle
import sys

from sampler import WAITING


def main():
    stacks = pickle.load(open(sys.argv[1], "rb"))
    top = int(sys.argv[2]) if len(sys.argv) > 2 else 60
    focus = sys.argv[3] if len(sys.argv) > 3 else None
    busy = [s for s in stacks if s and not any(w in s for w in WAITING)]
    print("samples %d, busy %d (%.0f%%)" % (len(stacks), len(busy), 100.0 * len(busy) / max(1, len(stacks))))
    if not busy:
        return
    if focus:
        within = [s for s in busy if focus in s]
        print("%s: %.1f%% of busy" % (focus, 100.0 * len(within) / len(busy)))
        callees = collections.Counter()
        for s in within:
            i = s.index(focus)
            callees[s[i - 1] if i > 0 else "<self>"] += 1
        for k, v in callees.most_common(top):
            print("  %5.1f%%  %s" % (100.0 * v / len(busy), k[:150]))
        return
    exclusive, inclusive = collections.Counter(), collections.Counter()
    for s in busy:
        exclusive[s[0]] += 1
        inclusive.update(set(s))
    print("== exclusive (busy)")
    for k, v in exclusive.most_common(top // 2):
        print("%5.1f%%  %s" % (100.0 * v / len(busy), k[:150]))
    print("== inclusive (busy)")
    for k, v in inclusive.most_common(top):
        print("%5.1f%%  %s" % (100.0 * v / len(busy), k[:150]))


if __name__ == "__main__":
    main()
