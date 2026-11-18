@echo off
rem Launches the real C++ site (liquidgov_http) inside WSL Ubuntu — not
rem preview.py. preview.py is a disconnected toy: no auth, no FileStore,
rem no jukebox/avatar/mail-attachment work. This assumes the canonical
rem WSL layout, ~/lg/liquidgov-platform (see build.txt).

where wsl >nul 2>nul
if errorlevel 1 (
    echo WSL was not found. liquidgov_http is a Linux binary built for WSL Ubuntu.
    echo Install WSL first: https://learn.microsoft.com/windows/wsl/install
    pause
    exit /b 1
)

echo Starting LiquidGov.US - the real C++ site, port 8080.
echo This is NOT the Python preview. Leave this window open.
echo In your browser open:
echo   http://127.0.0.1:8080
echo.

wsl bash -lc "cd ~/lg/liquidgov-platform && test -x $HOME/liquidgov_http || bash wsl-setup.sh; LIQUIDGOV_STORE=$HOME/lg/liquidgov_accounts.lgp LIQUIDGOV_BLOBS=$HOME/lg/liquidgov_blobs $HOME/liquidgov_http"

pause
