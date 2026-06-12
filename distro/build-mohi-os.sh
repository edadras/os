#!/bin/bash
# MOHI OS builder
#
# Builds a bootable live ISO of MOHI OS: an Ubuntu-based distribution with
# a Windows-style XFCE desktop, web browser, Waydroid (Android apps) and
# MOHI branding. Run as root on Ubuntu 24.04:
#
#   sudo bash distro/build-mohi-os.sh
#
# Output: mohi-os.iso (hybrid BIOS+UEFI, bootable from USB/DVD/VM)

set -euo pipefail

SUITE=noble
MIRROR=http://archive.ubuntu.com/ubuntu
WORK="${MOHI_WORK:-/srv/mohi-build}"
ROOTFS="$WORK/rootfs"
ISODIR="$WORK/iso"
OUT="${MOHI_OUT:-$PWD/mohi-os.iso}"
OS_PRETTY="MOHI OS"
OS_VER="1.1"

stage() { echo; echo "==== [$(date +%H:%M:%S)] $* ===="; }

in_chroot() {
    DEBIAN_FRONTEND=noninteractive chroot "$ROOTFS" /bin/bash -c "$1"
}

cleanup_mounts() {
    for m in dev/pts dev sys proc; do
        umount -lf "$ROOTFS/$m" 2>/dev/null || true
    done
}
trap cleanup_mounts EXIT

# ---------------------------------------------------------------- host deps
stage "Installing build dependencies on host"
apt-get install -y -qq debootstrap squashfs-tools xorriso \
    grub-pc-bin grub-efi-amd64-bin mtools dosfstools >/dev/null

# ---------------------------------------------------------------- bootstrap
if [ ! -e "$ROOTFS/etc/os-release" ]; then
    stage "Bootstrapping Ubuntu $SUITE base system"
    mkdir -p "$ROOTFS"
    debootstrap --arch=amd64 "$SUITE" "$ROOTFS" "$MIRROR"
else
    stage "Base system already present, skipping debootstrap"
fi

stage "Mounting virtual filesystems"
mount -t proc proc "$ROOTFS/proc" 2>/dev/null || true
mount -t sysfs sys "$ROOTFS/sys" 2>/dev/null || true
mount --bind /dev "$ROOTFS/dev" 2>/dev/null || true
mount --bind /dev/pts "$ROOTFS/dev/pts" 2>/dev/null || true

# Keep services from starting inside the chroot
printf '#!/bin/sh\nexit 101\n' > "$ROOTFS/usr/sbin/policy-rc.d"
chmod +x "$ROOTFS/usr/sbin/policy-rc.d"
cp /etc/resolv.conf "$ROOTFS/etc/resolv.conf"

# If the build host sits behind a TLS-intercepting proxy, trust its CA
# inside the chroot for the duration of the build (removed in cleanup).
if ls /usr/local/share/ca-certificates/*.crt >/dev/null 2>&1; then
    mkdir -p "$ROOTFS/usr/local/share/ca-certificates/build-proxy"
    cp /usr/local/share/ca-certificates/*.crt \
        "$ROOTFS/usr/local/share/ca-certificates/build-proxy/"
fi

# ---------------------------------------------------------------- apt setup
stage "Configuring apt sources"
cat > "$ROOTFS/etc/apt/sources.list" <<EOF
deb $MIRROR $SUITE main restricted universe multiverse
deb $MIRROR $SUITE-updates main restricted universe multiverse
deb $MIRROR $SUITE-security main restricted universe multiverse
EOF
in_chroot "apt-get update -qq"

# ---------------------------------------------------------------- packages
stage "Installing kernel, live-boot support and desktop (this takes a while)"
# drop the old XFCE desktop if this tree was built by an earlier version
in_chroot "apt-get purge -y -qq xfce4 xfce4-panel xfce4-session xfce4-settings \
    xfdesktop4 xfwm4 xfce4-terminal xfce4-whiskermenu-plugin \
    xfce4-taskmanager thunar mousepad ristretto \
    lightdm lightdm-gtk-greeter 2>/dev/null; \
    apt-get autoremove -y -qq --purge 2>/dev/null" || true
rm -rf "$ROOTFS/etc/xdg/xfce4" "$ROOTFS/etc/lightdm" \
    "$ROOTFS/usr/share/themes/Chicago95" "$ROOTFS"/usr/share/icons/Chicago95*

in_chroot "apt-get install -y -qq \
    linux-generic casper \
    locales sudo nano less ca-certificates curl git wget \
    plasma-desktop plasma-workspace-wayland kwin-wayland \
    plasma-nm plasma-pa kscreen \
    sddm dolphin konsole \
    network-manager \
    pulseaudio pavucontrol \
    epiphany-browser \
    fonts-dejavu fonts-vazirmatn \
    python3-pil \
    lxc dnsmasq-base nftables python3-pip \
    build-essential pkg-config libglib2.0-dev python3-dev cython3"
echo "/usr/bin/sddm" > "$ROOTFS/etc/X11/default-display-manager"

in_chroot "locale-gen en_US.UTF-8 fa_IR.UTF-8 >/dev/null || locale-gen en_US.UTF-8 >/dev/null"
in_chroot "update-ca-certificates >/dev/null 2>&1 || true"

# ---------------------------------------------------------------- waydroid
stage "Installing Waydroid (Android app support)"
set +e
in_chroot "curl -sf https://repo.waydro.id/waydroid.gpg \
        -o /usr/share/keyrings/waydroid.gpg && \
    echo 'deb [signed-by=/usr/share/keyrings/waydroid.gpg] https://repo.waydro.id/ $SUITE main' \
        > /etc/apt/sources.list.d/waydroid.list && \
    apt-get update -qq && apt-get install -y -qq waydroid"
WAYDROID_REPO=$?
set -e

if [ $WAYDROID_REPO -ne 0 ]; then
    stage "Waydroid repo unreachable - building from source instead"
    in_chroot "rm -f /etc/apt/sources.list.d/waydroid.list && apt-get update -qq"
    in_chroot "set -e
        cd /tmp
        rm -rf libglibutil libgbinder gbinder-python waydroid
        git clone -q --depth 1 https://github.com/sailfishos/libglibutil.git
        git clone -q --depth 1 https://github.com/mer-hybris/libgbinder.git
        git clone -q --depth 1 https://github.com/erfanoabdi/gbinder-python.git
        git clone -q --depth 1 https://github.com/waydroid/waydroid.git
        make -s -C libglibutil KEEP_VERSIONS=1 release pkgconfig
        make -s -C libglibutil install-dev
        make -s -C libgbinder KEEP_VERSIONS=1 release pkgconfig
        make -s -C libgbinder install-dev
        ldconfig
        cd gbinder-python && pip3 install -q --break-system-packages . && cd ..
        cd waydroid && make install && cd ..
        rm -rf /tmp/libglibutil /tmp/libgbinder /tmp/gbinder-python /tmp/waydroid"
fi

# binder kernel module sanity check
if find "$ROOTFS/lib/modules" -name 'binder*' -o -name '*binderfs*' | grep -q .; then
    echo "binder kernel module: OK"
else
    echo "WARNING: no binder module found in the kernel - Waydroid may not start"
fi

# ---------------------------------------------------------------- theme
stage "Installing Windows 11 style theme (Win11OS-kde + Fluent icons)"
in_chroot "set -e
    cd /tmp
    rm -rf Win11OS-kde Fluent-icon-theme
    git clone -q --depth 1 https://github.com/yeyushengfan258/Win11OS-kde.git
    git clone -q --depth 1 https://github.com/vinceliuice/Fluent-icon-theme.git
    cd Win11OS-kde && ./install.sh >/dev/null 2>&1 && cd ..
    cd Fluent-icon-theme && ./install.sh >/dev/null 2>&1 && cd ..
    rm -rf /tmp/Win11OS-kde /tmp/Fluent-icon-theme"

# ---------------------------------------------------------------- branding
stage "Applying MOHI branding"

cat > "$ROOTFS/etc/os-release" <<EOF
PRETTY_NAME="$OS_PRETTY $OS_VER"
NAME="$OS_PRETTY"
VERSION_ID="$OS_VER"
VERSION="$OS_VER"
ID=mohi
ID_LIKE="ubuntu debian"
HOME_URL="https://github.com/edadras/os"
EOF
cat > "$ROOTFS/etc/lsb-release" <<EOF
DISTRIB_ID=MOHI
DISTRIB_RELEASE=$OS_VER
DISTRIB_CODENAME=$SUITE
DISTRIB_DESCRIPTION="$OS_PRETTY $OS_VER"
EOF
echo "mohi" > "$ROOTFS/etc/hostname"
printf '127.0.0.1\tlocalhost\n127.0.1.1\tmohi\n' > "$ROOTFS/etc/hosts"

# wallpaper: Windows-11-style soft gradient "bloom"
mkdir -p "$ROOTFS/usr/share/backgrounds/mohi"
in_chroot "python3 - <<'PYEOF'
from PIL import Image, ImageDraw, ImageFont, ImageFilter
W, H = 1920, 1080
img = Image.new('RGB', (W, H))
d = ImageDraw.Draw(img)
c1, c2 = (16, 36, 94), (66, 24, 120)      # deep blue -> violet
for y in range(H):
    t = y / H
    d.line([(0, y), (W, y)], fill=tuple(int(a+(b-a)*t) for a, b in zip(c1, c2)))
glow = Image.new('RGB', (W, H), (0, 0, 0))
gd = ImageDraw.Draw(glow)
gd.ellipse((W//2-650, H//2-350, W//2+650, H//2+550), fill=(30, 90, 200))
gd.ellipse((W//2-300, H//2-450, W//2+700, H//2+150), fill=(70, 50, 180))
glow = glow.filter(ImageFilter.GaussianBlur(220))
img = Image.blend(img, Image.composite(glow, img, glow.convert('L')), 0.55)
d = ImageDraw.Draw(img)
f1 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 190)
f2 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 44)
t1 = 'MOHI'
w1 = d.textbbox((0, 0), t1, font=f1)[2]
d.text(((W-w1)//2+5, H//2-150+5), t1, font=f1, fill=(10, 15, 40))
d.text(((W-w1)//2, H//2-150), t1, font=f1, fill=(245, 248, 255))
t2 = 'Operating System'
w2 = d.textbbox((0, 0), t2, font=f2)[2]
d.text(((W-w2)//2, H//2+80), t2, font=f2, fill=(200, 210, 240))
img.save('/usr/share/backgrounds/mohi/wallpaper.png')
PYEOF"

# Plasma defaults: Windows-11-style centered bottom panel + Win11OS theme
SKEL="$ROOTFS/etc/skel/.config"
mkdir -p "$SKEL"

CSFILE=$(ls "$ROOTFS/usr/share/color-schemes/" 2>/dev/null | grep -i '^win11.*light' | head -1)
COLORSCHEME="${CSFILE%.colors}"
[ -z "$COLORSCHEME" ] && COLORSCHEME=BreezeLight

cat > "$SKEL/kdeglobals" <<EOF
[KDE]
LookAndFeelPackage=com.github.yeyushengfan258.Win11OS-light
widgetStyle=Breeze

[General]
ColorScheme=$COLORSCHEME

[Icons]
Theme=Fluent
EOF

cat > "$SKEL/plasmarc" <<'EOF'
[Theme]
name=Win11OS-light
EOF

cat > "$SKEL/kwinrc" <<'EOF'
[org.kde.kdecoration2]
library=org.kde.kwin.aurorae
theme=__aurorae__svg__Win11OS-light
EOF

cat > "$SKEL/plasma-org.kde.plasma.desktop-appletsrc" <<'EOF'
[Containments][1]
activityId=
formfactor=0
immutability=1
lastScreen=0
location=0
plugin=org.kde.plasma.folder
wallpaperplugin=org.kde.image

[Containments][1][Wallpaper][org.kde.image][General]
Image=file:///usr/share/backgrounds/mohi/wallpaper.png

[Containments][2]
activityId=
formfactor=2
immutability=1
lastScreen=0
location=4
plugin=org.kde.panel

[Containments][2][General]
AppletOrder=8;3;4;9;5;6;7

[Containments][2][Applets][3]
immutability=1
plugin=org.kde.plasma.kickoff

[Containments][2][Applets][4]
immutability=1
plugin=org.kde.plasma.icontasks

[Containments][2][Applets][5]
immutability=1
plugin=org.kde.plasma.marginsseparator

[Containments][2][Applets][6]
immutability=1
plugin=org.kde.plasma.systemtray

[Containments][2][Applets][7]
immutability=1
plugin=org.kde.plasma.digitalclock

[Containments][2][Applets][8]
immutability=1
plugin=org.kde.plasma.panelspacer

[Containments][2][Applets][9]
immutability=1
plugin=org.kde.plasma.panelspacer
EOF

# live session configuration
cat > "$ROOTFS/etc/casper.conf" <<EOF
export USERNAME="mohi"
export USERFULLNAME="MOHI Live User"
export HOST="mohi"
export BUILD_SYSTEM="Ubuntu"
export FLAVOUR="MOHI"
EOF

# SDDM: autologin into the Plasma Wayland session (needed so Android
# apps open as native windows via Waydroid multi-window mode)
WLSESSION=$(ls "$ROOTFS/usr/share/wayland-sessions/" 2>/dev/null | grep -i plasma | head -1)
SESSION_NAME="${WLSESSION%.desktop}"
[ -z "$SESSION_NAME" ] && SESSION_NAME=plasma
mkdir -p "$ROOTFS/etc/sddm.conf.d"
cat > "$ROOTFS/etc/sddm.conf.d/10-mohi-live.conf" <<EOF
[Autologin]
User=mohi
Session=$SESSION_NAME
EOF
if [ -d "$ROOTFS/usr/share/sddm/themes/Win11OS-light" ]; then
    printf '\n[Theme]\nCurrent=Win11OS-light\n' \
        >> "$ROOTFS/etc/sddm.conf.d/10-mohi-live.conf"
fi

# Android setup helper: init Waydroid + enable multi-window so Android
# apps get their own start-menu entries and open as normal windows
cat > "$ROOTFS/usr/local/bin/mohi-android-setup" <<'EOF'
#!/bin/bash
set -e
echo "=== MOHI Android Setup ==="
echo "Downloading the Android system image (needs internet)..."
sudo waydroid init -s GAPPS
if ! grep -q multi_windows /var/lib/waydroid/waydroid_base.prop 2>/dev/null; then
    echo "persist.waydroid.multi_windows=true" | \
        sudo tee -a /var/lib/waydroid/waydroid_base.prop >/dev/null
fi
sudo systemctl restart waydroid-container
nohup waydroid session start >/dev/null 2>&1 &
sleep 8
echo
echo "Done! Android apps now appear in the start menu under Waydroid"
echo "and open in their own windows. Install apps from the Play Store"
echo "or with:  waydroid app install yourapp.apk"
read -rp "Press Enter to close..."
EOF
chmod +x "$ROOTFS/usr/local/bin/mohi-android-setup"

mkdir -p "$ROOTFS/etc/skel/Desktop"
cat > "$ROOTFS/etc/skel/Desktop/setup-android.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Setup Android Apps
Comment=Download the Android system image and enable Android apps
Icon=phone
Exec=konsole -e /usr/local/bin/mohi-android-setup
Terminal=false
EOF
chmod +x "$ROOTFS/etc/skel/Desktop/setup-android.desktop"

# ---------------------------------------------------------------- cleanup
stage "Cleaning up chroot"
in_chroot "apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* || true"
truncate -s 0 "$ROOTFS/etc/machine-id"
rm -f "$ROOTFS/etc/resolv.conf"
if [ -d "$ROOTFS/usr/local/share/ca-certificates/build-proxy" ]; then
    rm -rf "$ROOTFS/usr/local/share/ca-certificates/build-proxy"
    in_chroot "update-ca-certificates --fresh >/dev/null 2>&1 || true"
fi

stage "Regenerating initramfs with casper hooks"
mount -t proc proc "$ROOTFS/proc" 2>/dev/null || true
in_chroot "update-initramfs -u -k all"

cleanup_mounts
trap - EXIT

# ---------------------------------------------------------------- ISO
stage "Building squashfs (this takes a while)"
mkdir -p "$ISODIR/casper" "$ISODIR/boot/grub" "$ISODIR/.disk"
rm -f "$ISODIR/casper/filesystem.squashfs"
mksquashfs "$ROOTFS" "$ISODIR/casper/filesystem.squashfs" \
    -comp zstd -Xcompression-level 19 -noappend -quiet -wildcards \
    -e "proc/*" "sys/*" "dev/*" "run/*" "tmp/*"

printf "%s" "$(du -sx --block-size=1 "$ROOTFS" | cut -f1)" \
    > "$ISODIR/casper/filesystem.size"

cp "$ROOTFS"/boot/vmlinuz-* "$ISODIR/casper/vmlinuz"
cp "$ROOTFS"/boot/initrd.img-* "$ISODIR/casper/initrd"
echo "$OS_PRETTY $OS_VER" > "$ISODIR/.disk/info"

cat > "$ISODIR/boot/grub/grub.cfg" <<EOF
set timeout=5
set default=0

menuentry "$OS_PRETTY $OS_VER (Live)" {
    linux /casper/vmlinuz boot=casper quiet splash ---
    initrd /casper/initrd
}

menuentry "$OS_PRETTY $OS_VER (safe graphics)" {
    linux /casper/vmlinuz boot=casper nomodeset quiet splash ---
    initrd /casper/initrd
}
EOF

stage "Creating hybrid BIOS+UEFI ISO"
grub-mkrescue -o "$OUT" "$ISODIR" -volid MOHI_OS 2>/dev/null

stage "DONE: $OUT"
ls -lh "$OUT"
