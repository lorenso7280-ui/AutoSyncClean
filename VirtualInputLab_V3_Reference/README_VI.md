# Virtual Input Lab V3

Bo thu nghiem Win32 doc lap danh cho ung dung do chung ta so huu. V3 khong hook,
khong inject DLL va khong dieu khien tro choi ben thu ba.

## Chuc nang V3

- Bon nut BUOC 1, BUOC 2, BUOC 3, BUOC 4 co hit-test rieng.
- Lay diem tren mot Target mau: dat con tro vao nut roi nhan F1, F2, F3, F4.
- Diem duoc luu theo toa do chuan hoa 0..10000.
- Moi buoc phat MOVE, LEFT DOWN, giu nut, roi LEFT UP.
- Chay chuoi 1-2-3-4 tren tat ca Target duoc danh dau.
- Co so vong, gian cach, tam dung, tiep tuc, dung va dat lai bo dem.
- Target da dong duoc ghi FAIL va bi bo qua, khong lam Controller treo.
- Ghi file VirtualInputLab_V3_log.csv voi PID, sequence, step, loai lenh,
  OK/FAIL va thoi gian gui.
- Khi phat lenh IPC, con tro Windows khong bi di chuyen.

## Build

Workflow GitHub Actions cu van dung duoc vi ten hai EXE khong thay doi. Upload
thu muc VirtualInputLab, commit, mo Actions va tai artifact
VirtualInputLab_Windows_x64.

## Kiem thu

1. Dong tat ca EXE V2.
2. Mo VirtualInputTarget.exe 6 lan va doi kich thuoc vai cua so.
3. Mo VirtualInputController.exe, bam Lam moi danh sach.
4. Dam bao du 6 PID va ca 6 duoc danh dau.
5. Tren mot Target mau, dat chuot lan luot vao tam BUOC 1..4 va nhan F1..F4.
6. Bam Dat lai bo dem.
7. Dat so vong 10, giu nut 80 ms, gian cach 250 ms.
8. Bam Bat dau 1-2-3-4.
9. Ket qua dung: moi nut cua ca 6 Target co so lan 10, sequence cuoi giong
   nhau va con tro vat ly van dung binh thuong.
10. Mo file CSV: cac dong binh thuong la OK; Target bi dong giua luc chay se co
    FAIL nhung Controller van tiep tuc.

## Pham vi

Ket qua chi chung minh IPC va toa do ao voi Target chu dong ho tro giao thuc V3.
Doomsday khong co bo nhan nay, nen hai EXE V3 khong dieu khien truc tiep game.
