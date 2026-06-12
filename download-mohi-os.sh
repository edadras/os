#!/bin/bash
set -e
echo "=== MOHI OS 1.1 Downloader ==="
for i in $(seq 0 89); do
  n=$(printf "%03d" "$i")
  echo "--- part $n / 89"
  curl -L --retry 5 -o "mohi-os.iso.$n" "https://raw.githubusercontent.com/edadras/os/release-files/parts/mohi-os.iso.$n"
done
cat mohi-os.iso.0* > mohi-os.iso && rm -f mohi-os.iso.0[0-9][0-9]
echo "Done! mohi-os.iso is ready."
