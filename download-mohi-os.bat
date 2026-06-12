@echo off
echo === MOHI OS 1.1 Downloader ===
echo Downloading 90 parts (2.2 GB total). This may take a while...
setlocal enabledelayedexpansion
for /L %%i in (0,1,89) do (
  set "n=00%%i"
  set "n=!n:~-3!"
  echo --- part !n! / 89
  curl -L --retry 5 -o "mohi-os.iso.!n!" "https://raw.githubusercontent.com/edadras/os/release-files/parts/mohi-os.iso.!n!"
)
echo Joining parts...
copy /b mohi-os.iso.000+mohi-os.iso.001+mohi-os.iso.002+mohi-os.iso.003+mohi-os.iso.004+mohi-os.iso.005+mohi-os.iso.006+mohi-os.iso.007+mohi-os.iso.008+mohi-os.iso.009+mohi-os.iso.010+mohi-os.iso.011+mohi-os.iso.012+mohi-os.iso.013+mohi-os.iso.014+mohi-os.iso.015+mohi-os.iso.016+mohi-os.iso.017+mohi-os.iso.018+mohi-os.iso.019+mohi-os.iso.020+mohi-os.iso.021+mohi-os.iso.022+mohi-os.iso.023+mohi-os.iso.024+mohi-os.iso.025+mohi-os.iso.026+mohi-os.iso.027+mohi-os.iso.028+mohi-os.iso.029+mohi-os.iso.030+mohi-os.iso.031+mohi-os.iso.032+mohi-os.iso.033+mohi-os.iso.034+mohi-os.iso.035+mohi-os.iso.036+mohi-os.iso.037+mohi-os.iso.038+mohi-os.iso.039+mohi-os.iso.040+mohi-os.iso.041+mohi-os.iso.042+mohi-os.iso.043+mohi-os.iso.044+mohi-os.iso.045+mohi-os.iso.046+mohi-os.iso.047+mohi-os.iso.048+mohi-os.iso.049+mohi-os.iso.050+mohi-os.iso.051+mohi-os.iso.052+mohi-os.iso.053+mohi-os.iso.054+mohi-os.iso.055+mohi-os.iso.056+mohi-os.iso.057+mohi-os.iso.058+mohi-os.iso.059+mohi-os.iso.060+mohi-os.iso.061+mohi-os.iso.062+mohi-os.iso.063+mohi-os.iso.064+mohi-os.iso.065+mohi-os.iso.066+mohi-os.iso.067+mohi-os.iso.068+mohi-os.iso.069+mohi-os.iso.070+mohi-os.iso.071+mohi-os.iso.072+mohi-os.iso.073+mohi-os.iso.074+mohi-os.iso.075+mohi-os.iso.076+mohi-os.iso.077+mohi-os.iso.078+mohi-os.iso.079+mohi-os.iso.080+mohi-os.iso.081+mohi-os.iso.082+mohi-os.iso.083+mohi-os.iso.084+mohi-os.iso.085+mohi-os.iso.086+mohi-os.iso.087+mohi-os.iso.088+mohi-os.iso.089 mohi-os.iso >nul
del mohi-os.iso.0??
echo Done! mohi-os.iso is ready (about 2.2 GB).
echo Write it to a USB stick with Rufus in DD mode, then boot from USB.
pause
