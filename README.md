This was designed for Windows systems.

Things needed:
make
gcc

How I got them:
1. Open Powershell
2. Run the following commands:
  a. Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
     Invoke-RestMethod -Uri https://get.scoop.sh | Invoke-Expression
  b. scoop bucket add main
  c. scoop install make
  d. scoop install gcc
