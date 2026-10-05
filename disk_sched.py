#!/usr/bin/env python3
"""Disk Scheduling Simulator (Terminal version)
FCFS, SSTF, SCAN, C-SCAN, LOOK, C-LOOK

ตัวอย่าง:
  python3 disk_sched.py                       # โหมดถามทีละค่า (interactive)
  python3 disk_sched.py -q "98 183 37 122 14 124 65 67" -H 53 -n 200
  python3 disk_sched.py -q 98,183,37 -H 53 -a SCAN -d down --graph
"""
import argparse
import random
import sys

ALGS = ["FCFS", "SSTF", "SCAN", "C-SCAN", "LOOK", "C-LOOK"]


def schedule(alg, reqs, head, n, direction):
    """คืนค่า (path, jump) โดย path คือลำดับตำแหน่งหัวอ่านเริ่มจาก head
    jump คือระยะกระโดดกลับของ C-SCAN / C-LOOK (0 ถ้าไม่มี)"""
    mx = n - 1
    p = [head]
    same = [x for x in reqs if x == head]              # อยู่ตรงหัวอ่านพอดี: เสิร์ฟทันที
    left = sorted([x for x in reqs if x < head], reverse=True)
    right = sorted([x for x in reqs if x > head])
    up = direction == "up"
    near, far = (right, left) if up else (left, right)
    edge, other_edge = (mx, 0) if up else (0, mx)
    jump = 0

    if alg == "FCFS":
        p += reqs
    elif alg == "SSTF":
        pool, c = list(reqs), head
        while pool:
            c = min(pool, key=lambda x: abs(x - c))   # เสมอกันเลือกตัวที่มาก่อนในคิว
            pool.remove(c)
            p.append(c)
    elif alg == "SCAN":
        p += same + near
        if far:
            if p[-1] != edge:
                p.append(edge)
            p += far
    elif alg == "LOOK":
        p += same + near + far
    elif alg == "C-SCAN":
        p += same + near
        if far:
            if p[-1] != edge:
                p.append(edge)
            jump = abs(edge - other_edge)
            p.append(other_edge)
            p += far[::-1]
    elif alg == "C-LOOK":
        p += same + near
        if far:
            start = far[-1]
            jump = abs(p[-1] - start)
            p += far[::-1]
    else:
        raise ValueError(alg)
    return p, jump


def movement(path):
    return sum(abs(b - a) for a, b in zip(path, path[1:]))


def parse_queue(text):
    parts = text.replace(",", " ").split()
    if not parts:
        raise ValueError("คิวคำขอว่าง")
    return [int(x) for x in parts]


def validate(reqs, head, n):
    if n < 2:
        raise ValueError("จำนวน cylinder ต้องมากกว่า 1")
    bad = [x for x in reqs + [head] if not 0 <= x < n]
    if bad:
        raise ValueError(f"ค่าต้องอยู่ในช่วง 0 ถึง {n - 1} (พบ {bad})")


def ascii_graph(path, reqs, n, width=60):
    lines = []
    scale = lambda c: round(c / (n - 1) * width)
    lines.append("      0" + " " * (width - len(str(n - 1)) - 1) + str(n - 1))
    for i, c in enumerate(path):
        row = ["·"] * (width + 1)
        if i > 0:
            a, b = sorted((scale(path[i - 1]), scale(c)))
            for k in range(a, b + 1):
                row[k] = "─"
        mark = "H" if i == 0 else ("*" if c in reqs else "o")
        row[scale(c)] = mark
        lines.append(f"{i:>4}  " + "".join(row) + f"  {c}")
    lines.append("      H = จุดเริ่ม  * = คำขอ  o = ขอบดิสก์ (ไม่ใช่คำขอ)")
    return "\n".join(lines)


def run(reqs, head, n, direction, algs, count_jump, graph):
    print(f"\nคิว: {' '.join(map(str, reqs))}")
    print(f"หัวอ่านเริ่ม: {head}   cylinders: 0-{n - 1}   ทิศ: {direction}   "
          f"นับ jump: {'ใช่' if count_jump else 'ไม่'}\n")
    results = {}
    for a in algs:
        path, jump = schedule(a, reqs, head, n, direction)
        total = movement(path) - (0 if count_jump else jump)
        results[a] = (path, total)

    best = min(t for _, t in results.values())
    print(f"{'Algorithm':<8} {'Head movement':>14} {'Avg/req':>9}")
    print("-" * 34)
    for a, (path, total) in results.items():
        star = "  <- น้อยสุด" if total == best and len(algs) > 1 else ""
        print(f"{a:<8} {total:>14} {total / len(reqs):>9.2f}{star}")

    for a, (path, total) in results.items():
        print(f"\n[{a}] ลำดับ: " + " -> ".join(map(str, path)))
        if graph:
            print(ascii_graph(path, reqs, n))


def ask(prompt, default):
    s = input(f"{prompt} [{default}]: ").strip()
    return s or str(default)


def main():
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass
    ap = argparse.ArgumentParser(description="Disk Scheduling Simulator (Terminal)")
    ap.add_argument("-q", "--queue", help='คิวคำขอ เช่น "98 183 37" หรือ 98,183,37')
    ap.add_argument("-H", "--head", type=int, help="ตำแหน่งหัวอ่านเริ่มต้น")
    ap.add_argument("-n", "--cylinders", type=int, default=200, help="จำนวน cylinder (ค่าเริ่มต้น 200)")
    ap.add_argument("-d", "--direction", choices=["up", "down"], default="up", help="ทิศเริ่มต้นของ SCAN/LOOK")
    ap.add_argument("-a", "--algo", choices=ALGS + ["ALL"], default="ALL", type=str.upper, help="อัลกอริทึม (ค่าเริ่มต้น ALL)")
    ap.add_argument("--no-jump", action="store_true", help="ไม่นับระยะกระโดดกลับของ C-SCAN/C-LOOK")
    ap.add_argument("-g", "--graph", action="store_true", help="แสดงกราฟเส้นทางหัวอ่านแบบ ASCII")
    ap.add_argument("-r", "--random", type=int, metavar="K", help="สุ่มคำขอ K ตัว")
    args = ap.parse_args()

    try:
        if args.random:
            reqs = [random.randrange(args.cylinders) for _ in range(args.random)]
            head = args.head if args.head is not None else random.randrange(args.cylinders)
            n, d, algs, cj, g = args.cylinders, args.direction, ALGS, not args.no_jump, args.graph
            if args.algo != "ALL":
                algs = [args.algo]
        elif args.queue is None:          # interactive mode
            print("=== Disk Scheduling Simulator ===  (กด Enter เพื่อใช้ค่าในวงเล็บ)")
            reqs = parse_queue(ask("คิวคำขอ", "98 183 37 122 14 124 65 67"))
            head = int(ask("ตำแหน่งหัวอ่านเริ่มต้น", 53))
            n = int(ask("จำนวน cylinder", 200))
            d = ask("ทิศทาง up/down", "up").lower()
            if d not in ("up", "down"):
                raise ValueError("ทิศทางต้องเป็น up หรือ down")
            a = ask("อัลกอริทึม (" + "/".join(ALGS) + "/ALL)", "ALL").upper()
            if a not in ALGS + ["ALL"]:
                raise ValueError("ไม่รู้จักอัลกอริทึม " + a)
            algs = ALGS if a == "ALL" else [a]
            cj = ask("นับ jump ของ C-SCAN/C-LOOK? y/n", "y").lower().startswith("y")
            g = ask("แสดงกราฟ ASCII? y/n", "y").lower().startswith("y")
        else:
            if args.head is None:
                ap.error("ต้องระบุ -H/--head เมื่อใช้ -q")
            reqs, head, n, d = parse_queue(args.queue), args.head, args.cylinders, args.direction
            algs = ALGS if args.algo == "ALL" else [args.algo]
            cj, g = not args.no_jump, args.graph
        validate(reqs, head, n)
    except ValueError as e:
        print("ข้อผิดพลาด:", e, file=sys.stderr)
        sys.exit(1)
    except (KeyboardInterrupt, EOFError):
        print()
        sys.exit(0)

    run(reqs, head, n, d, algs, cj, g)


if __name__ == "__main__":
    main()
