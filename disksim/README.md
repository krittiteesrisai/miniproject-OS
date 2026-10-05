# Disk Scheduling Simulator

โปรแกรมจำลองการจัดลำดับ I/O ของดิสก์ (FCFS, SSTF, SCAN, C-SCAN, LOOK, C-LOOK) เขียนด้วยภาษา C บน Linux โดยใช้ POSIX system calls

## ติดตั้งและ build (Ubuntu เครื่องจริง)

```bash
chmod +x scripts/setup.sh
./scripts/setup.sh        # ติดตั้ง gcc/make, ตรวจชนิดดิสก์, build และรัน test
```

หรือ build เอง: `make` และทดสอบกับตัวอย่างในตำรา: `make test`

## วิธีใช้

| คำสั่ง | ทำอะไร |
|---|---|
| `./disksim run -a scan -A 300` | แสดงการเคลื่อนที่ของหัวอ่านทีละขั้น (animate 300 ms ต่อขั้น) |
| `./disksim compare -o result.csv` | รันทั้ง 6 อัลกอริทึมพร้อมกันใน 6 process แล้วเปรียบเทียบ |
| `./disksim live -a sstf -c 4 -k 8` | client 4 process ส่ง request ผ่าน shared memory ให้ scheduler |
| `./disksim info` | แสดง I/O scheduler จริงของเคอร์เนลจาก `/sys/block` |

ตัวเลือก input: `-r 98,183,37` (ระบุเอง), `-f traces/textbook.txt` (อ่านจากไฟล์), `-n 50 -S 7` (สุ่ม 50 ตัว, seed 7)
ตัวเลือกดิสก์: `-H 53` (ตำแหน่งเริ่ม), `-D 200` (จำนวน cylinder), `-d up|down`, `-J` (ไม่นับระยะกระโดดกลับของ C-SCAN/C-LOOK)

ค่าเริ่มต้นคือตัวอย่างจากตำรา Silberschatz: request 98, 183, 37, 122, 14, 124, 65, 67 หัวอ่านเริ่มที่ 53

## System calls ที่ใช้

| System call | ไฟล์ | ใช้ทำอะไร |
|---|---|---|
| `open`, `read`, `close` | input.c, sysinfo.c | อ่านไฟล์ request และไฟล์ใน `/sys/block` |
| `open`, `write` (`dprintf`) | compare.c | บันทึกผลเป็น CSV |
| `fork`, `_exit`, `waitpid` | compare.c, live.c | สร้าง process ต่ออัลกอริทึม และ client process |
| `pipe`, `read`, `write` | compare.c | ส่งผลจาก child กลับ parent (write ขนาด ≤ PIPE_BUF จึง atomic) |
| `shm_open`, `ftruncate`, `mmap`, `munmap`, `shm_unlink` | live.c | คิว request ที่ใช้ร่วมกันระหว่าง process |
| `sem_init` (pshared), `sem_wait`, `sem_post`, `sem_trywait`, `sem_timedwait` | live.c | bounded buffer แบบ producer/consumer |
| `sigaction`, `kill` | live.c | Ctrl+C หยุดอย่างปลอดภัย ปิด client และลบ shared memory |
| `nanosleep` | live.c, visual.c | จำลองเวลาให้บริการของดิสก์ และ animation |
| `clock_gettime` | live.c | วัด response time ของแต่ละ request |
| `ioctl(TIOCGWINSZ)`, `isatty` | visual.c | ปรับกราฟตามความกว้างของ terminal |
| `opendir`, `readdir` | sysinfo.c | ไล่ดู block device ทั้งหมด |
| `getpid` | ทุกโหมด | แสดง pid เพื่อให้เห็นว่ามีหลาย process จริง |

## โครงสร้าง

```
include/disksim.h   โครงสร้างข้อมูลที่ใช้ร่วมกัน
src/algorithms.c    อัลกอริทึมทั้ง 6 แบบ (เขียนแบบ online picker ใช้ได้ทั้ง batch และ live)
src/input.c         อ่าน request จากไฟล์ / list / สุ่ม
src/visual.c        วาดกราฟการเคลื่อนที่ของหัวอ่าน
src/compare.c       โหมดเปรียบเทียบ (fork + pipe)
src/live.c          โหมดหลาย process (shared memory + semaphore + signal)
src/sysinfo.c       อ่าน scheduler จริงจาก sysfs
src/main.c          จัดการ argument
tests/run_tests.sh  ตรวจผลกับค่าในตำรา
scripts/setup.sh    ติดตั้งและตรวจเครื่อง
```

## โมเดลเวลา

เวลาต่อ request = settle 1 ms + 0.05 ms ต่อ cylinder + rotational latency เฉลี่ย (ครึ่งรอบที่ 7200 RPM ≈ 4.17 ms)
ในโหมด live เวลานี้ถูกขยาย 20 เท่า (`-t`) เพื่อให้มองเห็นคิวสะสมได้ตอน demo

## ตรวจระหว่าง demo

ขณะรัน `live` เปิดอีก terminal แล้วใช้ `ls -l /dev/shm` เพื่อดู shared memory object และ `ps -f --forest -C disksim` เพื่อดูโครงสร้าง parent/child
