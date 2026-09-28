# Disk Scheduling Simulator

จำลองและเปรียบเทียบอัลกอริทึม I/O Scheduling ของดิสก์: FCFS, SSTF, SCAN, C-SCAN, LOOK, C-LOOK

เป็นเว็บแบบ static ไฟล์เดียว (`index.html`) ไม่มี build step ไม่มี dependency
ยกเว้นฟอนต์ IBM Plex Sans Thai จาก Google Fonts (ถ้าโหลดไม่ได้จะใช้ฟอนต์ระบบแทน)

## รันในเครื่อง
เปิด `index.html` ในเบราว์เซอร์ได้เลย หรือใช้ `python3 -m http.server 8000` แล้วเข้า http://localhost:8000

## Deploy -> https://krittiteesrisai.github.io/miniproject-OS/

**GitHub Pages**
1. สร้าง repo แล้ว push ไฟล์ `index.html` กับ `README.md`
2. Settings → Pages → Source: Deploy from a branch → เลือก `main` / `(root)`
3. รอสักครู่ จะได้ลิงก์ `https://<username>.github.io/<repo>/`

**Netlify**: ลากโฟลเดอร์นี้ไปวางที่ https://app.netlify.com/drop

**Vercel**: `npx vercel --prod` ในโฟลเดอร์นี้ (Framework Preset: Other)

**Cloudflare Pages**: เชื่อม repo, Build command ว่าง, Output directory `/`

## ผลตรวจสอบ (ตัวอย่างตำรา: คิว 98 183 37 122 14 124 65 67, หัวอ่านเริ่มที่ 53, 200 cylinders, ทิศขึ้น)
| อัลกอริทึม | Head movement |
|---|---|
| FCFS | 640 |
| SSTF | 236 |
| SCAN | 331 |
| C-SCAN (นับ jump) | 382 |
| LOOK | 299 |
| C-LOOK (นับ jump) | 322 |
