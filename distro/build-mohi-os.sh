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
OS_VER="1.0"

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
in_chroot "apt-get install -y -qq \
    linux-generic casper \
    locales sudo nano less ca-certificates curl git wget \
    xfce4 xfce4-terminal xfce4-whiskermenu-plugin xfce4-taskmanager \
    thunar mousepad ristretto \
    lightdm lightdm-gtk-greeter \
    network-manager network-manager-gnome \
    pulseaudio pavucontrol \
    epiphany-browser \
    fonts-dejavu fonts-vazirmatn \
    python3-pil \
    lxc dnsmasq-base nftables python3-pip \
    build-essential pkg-config libglib2.0-dev python3-dev cython3"

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
stage "Installing Windows-style theme (Chicago95)"
in_chroot "set -e
    cd /tmp
    rm -rf Chicago95
    git clone -q --depth 1 https://github.com/grassmunk/Chicago95.git
    cp -r Chicago95/Theme/Chicago95 /usr/share/themes/
    cp -r Chicago95/Icons/* /usr/share/icons/
    rm -rf /tmp/Chicago95"

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

# wallpaper
mkdir -p "$ROOTFS/usr/share/backgrounds/mohi"
in_chroot "python3 - <<'PYEOF'
from PIL import Image, ImageDraw, ImageFont
W, H = 1920, 1080
img = Image.new('RGB', (W, H))
d = ImageDraw.Draw(img)
top, bottom = (0, 64, 64), (0, 128, 128)
for y in range(H):
    t = y / H
    d.line([(0, y), (W, y)], fill=tuple(int(a+(b-a)*t) for a, b in zip(top, bottom)))
f1 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 220)
f2 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 48)
t1 = 'MOHI'
w1 = d.textbbox((0,0), t1, font=f1)[2]
d.text(((W-w1)//2+6, H//2-160+6), t1, font=f1, fill=(0,40,40))
d.text(((W-w1)//2, H//2-160), t1, font=f1, fill=(240,250,250))
t2 = 'Operating System'
w2 = d.textbbox((0,0), t2, font=f2)[2]
d.text(((W-w2)//2, H//2+90), t2, font=f2, fill=(180,220,220))
img.save('/usr/share/backgrounds/mohi/wallpaper.png')
PYEOF"

# XFCE defaults: bottom Windows-style panel, whisker start menu, theme
XDG="$ROOTFS/etc/xdg/xfce4/xfconf/xfce-perchannel-xml"
mkdir -p "$XDG"

cat > "$XDG/xfce4-panel.xml" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<channel name="xfce4-panel" version="1.0">
  <property name="configver" type="int" value="2"/>
  <property name="panels" type="array">
    <value type="int" value="1"/>
    <property name="panel-1" type="empty">
      <property name="position" type="string" value="p=8;x=0;y=0"/>
      <property name="length" type="uint" value="100"/>
      <property name="position-locked" type="bool" value="true"/>
      <property name="size" type="uint" value="36"/>
      <property name="plugin-ids" type="array">
        <value type="int" value="1"/>
        <value type="int" value="2"/>
        <value type="int" value="3"/>
        <value type="int" value="4"/>
        <value type="int" value="5"/>
      </property>
    </property>
  </property>
  <property name="plugins" type="empty">
    <property name="plugin-1" type="string" value="whiskermenu"/>
    <property name="plugin-2" type="string" value="tasklist"/>
    <property name="plugin-3" type="string" value="separator">
      <property name="expand" type="bool" value="true"/>
      <property name="style" type="uint" value="0"/>
    </property>
    <property name="plugin-4" type="string" value="systray"/>
    <property name="plugin-5" type="string" value="clock"/>
  </property>
</channel>
EOF

cat > "$XDG/xsettings.xml" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<channel name="xsettings" version="1.0">
  <property name="Net" type="empty">
    <property name="ThemeName" type="string" value="Chicago95"/>
    <property name="IconThemeName" type="string" value="Chicago95"/>
  </property>
</channel>
EOF

cat > "$XDG/xfwm4.xml" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<channel name="xfwm4" version="1.0">
  <property name="general" type="empty">
    <property name="theme" type="string" value="Chicago95"/>
    <property name="button_layout" type="string" value="O|HMC"/>
  </property>
</channel>
EOF

cat > "$XDG/xfce4-desktop.xml" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<channel name="xfce4-desktop" version="1.0">
  <property name="backdrop" type="empty">
    <property name="screen0" type="empty">
      <property name="monitorVirtual1" type="empty">
        <property name="workspace0" type="empty">
          <property name="last-image" type="string" value="/usr/share/backgrounds/mohi/wallpaper.png"/>
          <property name="image-style" type="int" value="5"/>
        </property>
      </property>
      <property name="monitorVirtual-1" type="empty">
        <property name="workspace0" type="empty">
          <property name="last-image" type="string" value="/usr/share/backgrounds/mohi/wallpaper.png"/>
          <property name="image-style" type="int" value="5"/>
        </property>
      </property>
    </property>
  </property>
</channel>
EOF

# live session configuration
cat > "$ROOTFS/etc/casper.conf" <<EOF
export USERNAME="mohi"
export USERFULLNAME="MOHI Live User"
export HOST="mohi"
export BUILD_SYSTEM="Ubuntu"
export FLAVOUR="MOHI"
EOF

mkdir -p "$ROOTFS/etc/lightdm/lightdm.conf.d"
cat > "$ROOTFS/etc/lightdm/lightdm.conf.d/10-mohi-live.conf" <<EOF
[Seat:*]
autologin-user=mohi
autologin-user-timeout=0
user-session=xfce
EOF

# Waydroid first-run helper on the desktop
mkdir -p "$ROOTFS/etc/skel/Desktop"
cat > "$ROOTFS/etc/skel/Desktop/setup-android.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Setup Android Apps (Waydroid)
Comment=Download the Android system image and start Waydroid
Icon=phone
Exec=xfce4-terminal -T "Android Setup" -e "bash -c 'sudo waydroid init -s GAPPS && sudo systemctl restart waydroid-container && waydroid show-full-ui; read -p \"Done. Press Enter...\"'"
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
