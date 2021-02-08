
Setup for Linux will vary depending on your distro. However, if you're looking at this, you probably already have a good sense of what you need. 
Linux is naturally oriented around development, so configuration comes pretty easily
You'll want:

- cmake
- ninja, or some other build system
- opengles headers & libraries
- vulkan
- java runtime
- clang, lldb, etc

You can skip graphics APIs you don't want. Where a graphics API can't be found, cmake should spit out some human readable error messages, and
hopefully it should be obvious what to do from there.

Probably the trickiest thing to get right on Linux maybe Android deployment.

