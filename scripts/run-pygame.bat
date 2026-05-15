@echo off
setlocal

cd /d "%~dp0\.."

set PYGAME_HIDE_SUPPORT_PROMPT=1

if not exist .venv (
  py -3 -m venv .venv
)

call .venv\Scripts\activate.bat

python -c "import pygame" >nul 2>nul || python -m pip install -r native\pygame\requirements.txt
python native\pygame\snooker_pygame.py %*
