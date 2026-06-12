# MOHI

سیستم‌عامل ۳۲ بیتی نوشته‌شده **از صفر** به زبان C و Assembly — بدون استفاده از لینوکس یا هیچ کرنل آماده‌ای. با **رابط گرافیکی پنجره‌ای** شبیه ویندوز.

A 32-bit operating system written **from scratch** in C and assembly — no Linux, no pre-made kernel. Featuring a **Windows-style graphical desktop**.

## امکانات / Features

### رابط گرافیکی (v0.2)
- دسکتاپ گرافیکی 1024×768 (فریم‌بافر VBE)
- تسک‌بار با دکمه استارت، دکمه پنجره‌های باز و **ساعت واقعی** (از RTC سخت‌افزار)
- منوی استارت با hover-highlight
- پنجره‌های قابل جابه‌جایی (drag از نوار عنوان)، دکمه بستن، مدیریت فوکوس و z-order
- درایور ماوس PS/2 با نشانگر گرافیکی
- پنجره ترمینال با شل کامل داخلش
- پنجره‌های About و System Info

### هسته
- بوت‌لودر GRUB با استاندارد Multiboot (بوت روی سخت‌افزار واقعی)
- کرنل اختصاصی ۳۲ بیتی (Protected Mode)
- مدیریت سگمنت‌ها (GDT) و وقفه‌ها (IDT + PIC)
- تایمر سخت‌افزاری PIT با فرکانس ۱۰۰ هرتز
- درایور کیبورد PS/2 (با Shift و Caps Lock)
- درایور متنی VGA (حالت fallback وقتی فریم‌بافر موجود نیست)
- پورت سریال COM1 برای دیباگ
- شل با دستورات: `help` `about` `logo` `clear` `echo` `uptime` `mem` `ticks` `color` `reboot` `halt`

## ساخت / Build

پیش‌نیازها (اوبونتو/دبیان):

```bash
sudo apt install build-essential nasm grub-pc-bin grub-common xorriso mtools qemu-system-x86
```

ساخت ISO:

```bash
make
```

خروجی: `mohi.iso`

## اجرا در شبیه‌ساز / Run in QEMU

```bash
make run
```

## نصب روی فلش و بوت سخت‌افزار واقعی / Boot on real hardware

⚠️ **هشدار:** دستور زیر همه اطلاعات فلش را پاک می‌کند. مطمئن شوید `sdX` دقیقاً فلش شماست (`lsblk` را اجرا کنید).

```bash
sudo dd if=mohi.iso of=/dev/sdX bs=4M status=progress conv=fsync
```

در ویندوز می‌توانید از [Rufus](https://rufus.ie) (حالت DD) یا balenaEtcher استفاده کنید.

سپس سیستم را با فلش بوت کنید:
1. هنگام روشن شدن سیستم وارد Boot Menu شوید (معمولاً F12 یا F11 یا Esc)
2. اگر سیستم شما UEFI است، در تنظیمات BIOS گزینه **Legacy Boot / CSM** را فعال کنید (این نسخه با بوت Legacy/BIOS کار می‌کند)
3. فلش USB را انتخاب کنید — MOHI بوت می‌شود!

💡 **توصیه:** برای امتحان اول، داخل VirtualBox یا VMware یک ماشین مجازی بسازید و `mohi.iso` را به‌عنوان CD بوت کنید.

## تغییر نام و برند / Rebranding

اسم و نسخه سیستم‌عامل در یک فایل متمرکز است: [`kernel/config.h`](kernel/config.h)

```c
#define OS_NAME    "MOHI"       /* اسم دلخواه خودتان */
#define OS_VERSION "0.2.0"
```

لوگوی ASCII در `kernel/shell.c` (تابع `print_logo`) و اسم منوی GRUB در `grub/grub.cfg` قابل تغییر است.

## ساختار پروژه / Project layout

```
boot/boot.asm          نقطه ورود Multiboot (با درخواست حالت گرافیکی)
kernel/kernel.c        راه‌اندازی کرنل (kmain)
kernel/gdt.c           جدول سگمنت‌ها (GDT)
kernel/idt.c           جدول وقفه‌ها، PIC و مدیریت exception
kernel/interrupts.asm  stub های وقفه (ISR/IRQ)
kernel/timer.c         تایمر PIT
kernel/keyboard.c      درایور کیبورد PS/2
kernel/mouse.c         درایور ماوس PS/2
kernel/rtc.c           ساعت سخت‌افزاری (CMOS RTC)
kernel/fb.c            درایور فریم‌بافر گرافیکی (32bpp)
kernel/font8x8.h       فونت بیتمپ 8x8 (Public Domain)
kernel/gui.c           دسکتاپ، مدیر پنجره، تسک‌بار، منوی استارت
kernel/vga.c           درایور متنی VGA (fallback) + هوک خروجی
kernel/serial.c        پورت سریال (دیباگ)
kernel/shell.c         شل و دستورات (هم متنی هم داخل GUI)
kernel/string.c        توابع پایه رشته/حافظه
linker.ld              اسکریپت لینکر (کرنل در آدرس 1MB)
grub/grub.cfg          منوی بوت GRUB
```

## قدم‌های بعدی پیشنهادی / Roadmap ideas

- مدیریت حافظه (paging + heap allocator)
- فایل‌سیستم ساده و برنامه File Manager
- برنامه‌های بیشتر داخل GUI (ماشین‌حساب، Notepad، بازی)
- اجرای برنامه‌های کاربر (user mode + syscall)
- چندوظیفگی (task switching)
- درایور شبکه و پشته TCP/IP ساده
