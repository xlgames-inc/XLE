
At a minimum, you need enough to compile and debug executables using a llvm, vscode, cmake & ninja environment
Other compilation environments are possible, but this is the simpliest and uses the least platform-specific parts

If you haven't set up a windows dev environment like this before; you'll probably need the things listed below

Brief install instructions for windows (using clang & vscode)
* install vscode
* install llvm for windows (I'm using 11.0.0, win64) from https://releases.llvm.org/download.html
* install cmake (I used choco install cmake)
* change vscode settings to point to cmake executable location
* install ninja (I used choco install ninja)
* install windows 10 sdk (https://developer.microsoft.com/en-us/windows/downloads/windows-10-sdk/)
* this gets you 99% of the way there... but it turns out you also need Visual Studio 2019 for 2 .lib files: oldnames.lib & msvcrtd.lib. Install from https://visualstudio.microsoft.com/downloads/. Be aware of the licensing requirements, however

Note that Git and ssh can be a hassle for windows. If you don't already have a setup you like, the following
is what I tend to do:
* install chocolatey (https://chocolatey.org/install)
* install cmder (choco install cmder)
* this brings with it git (in the folder C:\tools\Cmder\vendor\git-for-windows\bin)
* enable openssh for windows using "Add or remove programs"/"Optional features"/"Add feature" -> add "OpenSSH Client"
* this gives you ssh-keygen, etc. You should use that to generate a key called ~/.ssh/id_rsa
* git doesn't play well with the ssh-agent in Windows' OpenSSH Client
* install, you must run "start-ssh-agent" from within cmder. This is a script within the cmder install, and will start an agent and load that key
* you may want to setup the cmder scripts so that start-ssh-agent is called every time 
* you also need to setup the path to git in settings.json for vscode ("git.path": "C:\\tools\\Cmder\\vendor\\git-for-windows\\bin\\git.exe")

