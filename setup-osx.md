
Brief setup instructions for OSX (tested on a M1 Mac Mini with Big Sur)

- install xcode from the app store
- install homebrew (brew.sh)
- install vscode
- brew install cmake
- brew install ninja
- add vscode extensions: cmake tools, C/C++, Catch2 and Google Test Explorer
- configure using CMake Tools in vscode (clang 12.0 kit)

If you want to use remote debugging with vscode, you can do it, but you must run the following
command first on the OSX machine you're connecting to:
```/usr/sbin/DevToolsSecurity --enable```

OSX normally wants to popup a password challenge when lldb attempts to connect. But this password
challenge can't be completed over ssh. See also https://stackoverflow.com/questions/27238931/lldb-attaching-to-a-process-on-os-x-from-an-ssh-session
