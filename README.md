# Disk Scheduling Simulator

จำลองและเปรียบเทียบอัลกอริทึม I/O Scheduling ของดิสก์: FCFS, SSTF, SCAN, C-SCAN, LOOK, C-LOOK
มีให้ใช้ 3 แบบ คือ **โปรแกรมหลักภาษา C** (`disksim/`) ที่ใช้ POSIX system calls, **เว็บ** (`index.html`) และ **Terminal** (`disk_sched.py`)

🔗 เว็บออนไลน์: https://krittiteesrisai.github.io/miniproject-OS/

-———-

## โปรแกรมหลักภาษา C (`disksim/`)

โปรแกรม system-level บน Linux ใช้ POSIX system calls ด้าน process, pipe, shared memory, semaphore และ signal

### ติดตั้งและ build บน Ubuntu

```bash
git clone https://github.com/krittiteesrisai/miniproject-OS.git
cd miniproject-OS/disksim
chmod +x scripts/setup.sh tests/run_tests.sh
./scripts/setup.sh      # ติดตั้ง build-essential ตรวจดิสก์ของเครื่อง build และรันชุดทดสอบ
```

หรือ build เองด้วย `make` แล้วทดสอบด้วย `make test` (ตรวจกับค่าในตำรา 11 กรณี)

### 4 โหมดการทำงาน

| คำสั่ง | ทำอะไร |
|---|---|
| `./disksim run -a scan -A 300` | แสดงเส้นทางหัวอ่านทีละขั้นใน terminal (animate 300 ms ต่อขั้น) |
| `./disksim compare -o result.csv` | fork 6 child รันทั้ง 6 อัลกอริทึมพร้อมกัน ส่งผลกลับทาง pipe แล้วเปรียบเทียบ |
| `./disksim live -a sstf -c 4 -k 8` | client 4 process ส่งคำขอผ่านคิวใน shared memory ให้ scheduler (Ctrl+C เพื่อหยุด) |
| `./disksim info` | แสดงชนิดดิสก์และ I/O scheduler จริงของเครื่องจาก `/sys/block` |

| ตัวเลือก | ความหมาย | ค่าเริ่มต้น |
|---|---|---|
| `-a` | `fcfs` `sstf` `scan` `cscan` `look` `clook` | |
| `-r` | คิวคำขอ เช่น `-r 98,183,37` | คิวตัวอย่างในตำรา |
| `-f` | อ่านคิวจากไฟล์ เช่น `-f traces/textbook.txt` | |
| `-n`, `-S` | สุ่มคำขอ N ตัว และกำหนด seed | |
| `-H` | ตำแหน่งหัวอ่านเริ่มต้น | `53` |
| `-D` | จำนวน cylinder | `200` |
| `-d` | `up` หรือ `down` | `up` |
| `-J` | ไม่นับระยะกระโดดกลับของ C-SCAN/C-LOOK | นับ |
| `-c`, `-k`, `-t` | จำนวน client, คำขอต่อ client, ตัวคูณเวลา (โหมด live) | `3`, `6`, `20` |

### System calls ที่ใช้

| กลุ่ม | System calls | ไฟล์ | ใช้ทำอะไร |
|---|---|---|---|
| File I/O | `open`, `read`, `write`, `close` | input.c, sysinfo.c, compare.c | อ่านไฟล์คำขอและ sysfs เขียน CSV |
| Process | `fork`, `_exit`, `waitpid`, `getpid` | compare.c, live.c | child ต่ออัลกอริทึม และ client process |
| IPC | `pipe` | compare.c | child ส่งผลกลับ parent (เขียน ≤ `PIPE_BUF` จึง atomic) |
| IPC | `shm_open`, `ftruncate`, `mmap`, `munmap`, `shm_unlink` | live.c | คิวคำขอที่ทุก process ใช้ร่วมกัน |
| Synchronization | `sem_init`, `sem_wait`, `sem_post`, `sem_trywait`, `sem_timedwait` | live.c | bounded buffer แบบ producer/consumer |
| Signal | `sigaction`, `kill` | live.c | Ctrl+C หยุด client และลบ shared memory อย่างปลอดภัย |
| Time | `nanosleep`, `clock_gettime` | live.c, visual.c | จำลองเวลาดิสก์ วัด response time และ animation |
| Terminal | `ioctl(TIOCGWINSZ)`, `isatty` | visual.c | ปรับกราฟตามขนาดหน้าจอ |
| Directory | `opendir`, `readdir` | sysinfo.c | อ่านรายการ block device |

รายละเอียดโครงสร้างไฟล์และโมเดลเวลาอยู่ใน [`disksim/README.md`](disksim/README.md)

---

## วิธีใช้แบบเว็บ

เปิดลิงก์ด้านบน หรือดับเบิลคลิกไฟล์ `index.html` เปิดในเบราว์เซอร์ได้เลย ไม่ต้องติดตั้งอะไร

1. ใส่คิวคำขอ ตำแหน่งเริ่มของหัวอ่าน จำนวน cylinder และทิศเริ่มต้น (หรือกด **ตัวอย่างในตำรา**)
2. กด **คำนวณ** หรือ `Enter` โปรแกรมจะคิดทั้ง 6 อัลกอริทึมพร้อมกัน
3. เลือกอัลกอริทึมในช่อง **ดูกราฟของ** แล้วกด **เล่น** เพื่อดูหัวอ่านวิ่งทีละก้าว แขนหัวอ่านบนรูปจานดิสก์มุมขวาบนจะขยับตาม
4. ดูตาราง **เทียบทุกอัลกอริทึม** ว่าตัวไหนวิ่งน้อยที่สุด (คลิกแถวเพื่อสลับกราฟ)

| ช่อง | ความหมาย |
|---|---|
| คิวคำขอ | เลข cylinder ตามลำดับที่เข้ามา คั่นด้วยเว้นวรรคหรือ `,` |
| หัวอ่านเริ่มที่ | cylinder ที่หัวอ่านอยู่ก่อนเริ่ม |
| จำนวน cylinder | ดิสก์มี cylinder 0 ถึง N-1 |
| ทิศเริ่มต้น | มีผลกับ SCAN, C-SCAN, LOOK, C-LOOK |
| นับระยะกระโดดกลับ | นับระยะที่ C-SCAN/C-LOOK กระโดดกลับอีกฝั่งหรือไม่ |

| ปุ่ม | คีย์ลัด | ทำอะไร |
|---|---|---|
| คำนวณ | `Enter` | คำนวณใหม่ |
| ตัวอย่างในตำรา | | โหลดคิว 98 183 37 122 14 124 65 67, หัวอ่าน 53, 200 cylinders |
| สุ่มคำขอ | | สุ่มคำขอ 8 ตัว |
| เล่น / หยุด | `Space` | หัวอ่านเดินทีละก้าวอัตโนมัติ |
| ◀ ย้อน / ไป ▶ | `←` `→` | ถอยหรือเดินทีละก้าว |
| เริ่มใหม่ | `Home` | กลับไปก้าวที่ 0 |
| อ่านวิธีใช้ | `?` | เปิดคู่มือท้ายหน้า |

ในกราฟ จุดน้ำเงิน = จุดเริ่ม, จุดส้ม = คำขอ, จุดเทา = ขอบดิสก์ และเส้นประ = ช่วงที่ C-SCAN/C-LOOK กระโดดกลับ

---

## วิธีใช้แบบ Terminal (Python)

ต้องมี Python 3.7 ขึ้นไป (ไม่ต้องติดตั้ง library เพิ่ม)

```bash
git clone https://github.com/krittiteesrisai/miniproject-OS.git
cd miniproject-OS
```

> บน Windows ถ้าคำสั่ง `python3` ใช้ไม่ได้ ให้ใช้ `python` หรือ `py` แทน

**โหมดถามทีละค่า** (เหมาะกับคนที่ไม่อยากจำคำสั่ง) กด Enter เพื่อใช้ค่าตัวอย่างในวงเล็บ

```bash
python3 disk_sched.py
```

**โหมดใส่ค่าในคำสั่ง**

```bash
# เปรียบเทียบทุกอัลกอริทึม
python3 disk_sched.py -q "98 183 37 122 14 124 65 67" -H 53 -n 200

# ดูเฉพาะ SCAN ทิศลง พร้อมกราฟ ASCII
python3 disk_sched.py -q 98,183,37,122,14,124,65,67 -H 53 -a SCAN -d down -g

# สุ่มคำขอ 10 ตัว
python3 disk_sched.py -r 10 -g

# ดูตัวเลือกทั้งหมด
python3 disk_sched.py -h
```

| ตัวเลือก | ความหมาย | ค่าเริ่มต้น |
|---|---|---|
| `-q`, `--queue` | คิวคำขอ | (ถ้าไม่ใส่จะเข้าโหมดถามทีละค่า) |
| `-H`, `--head` | ตำแหน่งหัวอ่านเริ่มต้น | ต้องระบุเมื่อใช้ `-q` |
| `-n`, `--cylinders` | จำนวน cylinder | `200` |
| `-d`, `--direction` | `up` หรือ `down` | `up` |
| `-a`, `--algo` | `FCFS` `SSTF` `SCAN` `C-SCAN` `LOOK` `C-LOOK` หรือ `ALL` | `ALL` |
| `--no-jump` | ไม่นับระยะกระโดดกลับของ C-SCAN/C-LOOK | นับ |
| `-g`, `--graph` | แสดงกราฟเส้นทางหัวอ่านแบบ ASCII | ไม่แสดง |
| `-r K`, `--random K` | สุ่มคำขอ K ตัว | |

**ตัวอย่างผลลัพธ์**

```
Algorithm  Head movement   Avg/req
----------------------------------
FCFS                640     80.00
SSTF                236     29.50  <- น้อยสุด
SCAN                331     41.38
C-SCAN              382     47.75
LOOK                299     37.38
C-LOOK              322     40.25

[C-SCAN] ลำดับ: 53 -> 65 -> 67 -> 98 -> 122 -> 124 -> 183 -> 199 -> 0 -> 14 -> 37
```

---

## ผลตรวจสอบ
ตัวอย่างจากตำรา: คิว 98 183 37 122 14 124 65 67, หัวอ่านเริ่มที่ 53, 200 cylinders, ทิศขึ้น
(ทั้งโปรแกรม C เวอร์ชันเว็บ และ Terminal ได้ค่าตรงกัน)

| อัลกอริทึม | Head movement |
|---|---|
| FCFS | 640 |
| SSTF | 236 |
| SCAN | 331 |
| C-SCAN (นับ jump) | 382 |
| LOOK | 299 |
| C-LOOK (นับ jump) | 322 |

---

## โครงสร้าง repository

```
miniproject-OS/
├── disksim/                  โปรแกรมหลักภาษา C
│   ├── include/disksim.h
│   ├── src/                  algorithms, input, visual, compare, live, sysinfo, main
│   ├── tests/run_tests.sh    ชุดทดสอบกับค่าในตำรา
│   ├── scripts/setup.sh      ติดตั้งและตรวจเครื่อง
│   ├── traces/textbook.txt
│   └── Makefile
├── index.html                เว็บ
└── disk_sched.py             Terminal (Python)
```
