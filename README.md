
How does it work
----------------

Fluxcapacitor uses low-level system trickery to do its job. The tricks
are a combination of generic *nix API's and Linux-specifc
functionality. A list of acronyms: LD_PRELOAD, ptrace, signals.


0. (pin CPU)
1. fluxcapacitor launches (fork + execve) commands with LDPRELOAD
2. for every command fluxcapacitor_preload.so is loaded, and
   on start it does few things.

  a) it registers a dummy handler to SIGUSR2 signal
  
  b) it replaces gettimeofday(), time(), ftime(), clock_gettime(), nanosleep()
     functions with custom implementation (that falls back to a
     clock_gettime(CLOCK_REALTIME) syscall, and not a vdso call)

3. loader is also doing a ptrace on all the launched commands



