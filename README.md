# HeylelOS coreutils

  HeylelOS equivalent of the GNU coreutils.
Trying to implement the following utilities as described in the [POSIX.1-2017](http://pubs.opengroup.org/onlinepubs/9699919799/) specification.

## Why not take one from another OS?

  First I thought: "Because it would had external dependencies. It's still great to create a standalone.
Good for practice and educational purposes too."
  However, I know think that by creating a STRICTLY standard core-utils, developpers would gain something.
They will gain the fact that if their program runs on HeylelOS, it shall STRICTLY run on any other Unix/Linux.
So I will not add extensions except if those one are considered common to every other Unix/Linux (eg. echo -n).

NB: If you find an error, or you think one extension follows the previous exception clause,
do not hesitate to contact me.

## Utilities

- [x] asa
- [ ] awk Using the [original awk](https://github.com/onetrueawk/awk) for now.
- [x] basename
- [ ] bc Will do it later, maybe create a scriptutils package with all unix-core scripts
- [x] cal
- [x] cat
- [x] chgrp
- [x] chmod
- [x] chown
- [x] cksum
- [x] cmp
- [ ] comm
- [ ] command
- [ ] cp
- [ ] csplit
- [ ] cut
- [ ] date
- [ ] dd
- [ ] df
- [ ] diff
- [ ] dirname
- [ ] du
- [x] echo + echo-xsi (an XSI compliant echo, both behaviors are different, cf. Standard)
- [ ] expand
- [ ] expr
- [ ] false
- [ ] file
- [ ] find
- [ ] fold
- [ ] fuser
- [ ] gencat
- [ ] getconf
- [ ] grep
- [ ] head
- [ ] id
- [ ] ipcrm
- [ ] ipcs
- [ ] join
- [ ] kill
- [ ] link
- [ ] ln
- [ ] logger
- [ ] logname
- [ ] ls
- [ ] mesg
- [ ] mkdir
- [ ] mkfifo
- [ ] mv
- [ ] newgrp
- [ ] nice
- [ ] nl
- [ ] nohup
- [ ] od
- [ ] patch
- [ ] pathchk
- [ ] pr
- [ ] printf
- [ ] ps
- [ ] pwd
- [ ] renice
- [ ] rm
- [ ] rmdir
- [ ] sh Using [bash](https://github.com/gitGNU/gnu_bash) as a replacement
- [ ] sleep
- [ ] sort
- [ ] split
- [ ] strings
- [ ] stty
- [ ] tabs
- [ ] tail
- [ ] tee
- [ ] test
- [ ] time
- [ ] touch
- [ ] tput
- [ ] tr
- [ ] true
- [ ] tsort
- [ ] tty
- [ ] ulimit
- [ ] uname
- [x] uniq
- [x] unlink
- [ ] wc
- [ ] what
- [ ] who
- [ ] xargs

