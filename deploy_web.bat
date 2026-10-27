@echo off
REM Builds the web game and deploys it to the fly.io app sanguodream (docs/WEB.md).
REM Before the first deploy, set the password: flyctl secrets set SITE_PASSWORD=... -a sanguodream
REM The fly builder makes the image from the Dockerfile. Docker is not necessary on this computer.
setlocal
call "%~dp0build_web.bat"
if errorlevel 1 exit /b 1
pushd "%~dp0"
flyctl deploy --remote-only --ha=false
set ERR=%errorlevel%
popd
if not %ERR%==0 (
  echo deploy_web.bat: FAILED
  exit /b 1
)
echo deploy_web.bat: done, https://sanguodream.fly.dev
exit /b 0
