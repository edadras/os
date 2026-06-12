# AriaOS

سیستم‌عامل ۳۲ بیتی نوشته‌شده **از صفر** به زبان C و Assembly — بدون استفاده از لینوکس یا هیچ کرنل آماده‌ای.

A 32-bit operating system written **from scratch** in C and assembly — no Linux, no pre-made kernel.

## امکانات / Features

- بوت‌لودر GRUB با استاندارد Multiboot (بوت روی سخت‌افزار واقعی)
- کرنل اختصاصی ۳۲ بیتی (Protected Mode)
- مدیریت سگمنت‌ها (GDT) و وقفه‌ها (IDT + PIC)
- تایمر سخت‌افزاری PIT با فرکانس ۱۰۰ هرتز
- درایور کیبورد PS/2 (با پشتیبانی Shift و Caps Lock)
- درایور نمایشگر متنی VGA با اسکرول و رنگ
- پورت سریال COM1 برای دیباگ
- شل تعاملی با دستورات: `help` `about` `logo` `clear` `echo` `uptime` `mem` `ticks` `color` `reboot` `halt`

## ساخت / Build

پیش‌نیازها (اوبونتو/دبیان):

```bash
sudo apt install build-essential nasm grub-pc-bin grub-common xorriso mtools qemu-system-x86
```

ساخت ISO:

```bash
make
```

خروجی: `ariaos.iso`

## اجرا در شبیه‌ساز / Run in QEMU

```bash
make run
```

## نصب روی فلش و بوت سخت‌افزار واقعی / Boot on real hardware

⚠️ **هشدار:** دستور زیر همه اطلاعات فلش را پاک می‌کند. مطمئن شوید `sdX` دقیقاً فلش شماست (`lsblk` را اجرا کنید).

```bash
sudo dd if=ariaos.iso of=/dev/sdX bs=4M status=progress conv=fsync
```

در ویندوز می‌توانید از [Rufus](https://rufus.ie) (حالت DD) یا balenaEtcher استفاده کنید.

سپس سیستم را با فلش بوت کنید:
1. هنگام روشن شدن سیستم وارد Boot Menu شوید (معمولاً F12 یا F11 یا Esc)
2. اگر سیستم شما UEFI است، در تنظیمات BIOS گزینه **Legacy Boot / CSM** را فعال کنید (این نسخه از AriaOS با بوت Legacy/BIOS کار می‌کند)
3. فلش USB را انتخاب کنید — AriaOS بوت می‌شود!

💡 **توصیه:** برای امتحان اول، داخل VirtualBox یا VMware یک ماشین مجازی بسازید و `ariaos.iso` را به‌عنوان CD بوت کنید.

## تغییر نام و برند / Rebranding

اسم و نسخه سیستم‌عامل در یک فایل متمرکز است: [`kernel/config.h`](kernel/config.h)

```c
#define OS_NAME    "AriaOS"     /* اسم دلخواه خودتان */
#define OS_VERSION "0.1.0"
```

لوگوی ASCII در `kernel/shell.c` (تابع `print_logo`) و اسم منوی GRUB در `grub/grub.cfg` قابل تغییر است.

## ساختار پروژه / Project layout

```
boot/boot.asm          نقطه ورود Multiboot و استک اولیه
kernel/kernel.c        راه‌اندازی کرنل (kmain)
kernel/gdt.c           جدول سگمنت‌ها (GDT)
kernel/idt.c           جدول وقفه‌ها، PIC و مدیریت exception
kernel/interrupts.asm  stub های وقفه (ISR/IRQ)
kernel/timer.c         تایمر PIT
kernel/keyboard.c      درایور کیبورد PS/2
kernel/vga.c           درایور متنی VGA
kernel/serial.c        پورت سریال (دیباگ)
kernel/shell.c         شل تعاملی و دستورات
kernel/string.c        توابع پایه رشته/حافظه
linker.ld              اسکریپت لینکر (کرنل در آدرس 1MB)
grub/grub.cfg          منوی بوت GRUB
```

## قدم‌های بعدی پیشنهادی / Roadmap ideas

- مدیریت حافظه (paging + heap allocator)
- فایل‌سیستم ساده (مثلاً FAT یا فایل‌سیستم اختصاصی در RAM)
- حالت گرافیکی (VESA framebuffer) و رابط کاربری
- اجرای برنامه‌های کاربر (user mode + syscall)
- چندوظیفگی (task switching)
