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

- [ ] awk Using the [original awk](https://github.com/onetrueawk/awk) for now.
- [x] basename
- [ ] bc
- [x] cal
- [x] cat
- [x] chgrp
- [x] chmod
- [x] chown
- [x] cksum
- [x] cmp
- [ ] comm
- [x] cp
- [ ] csplit
- [ ] cut
- [ ] date
- [ ] dd
- [ ] df
- [ ] diff
- [x] dirname
- [ ] du
- [x] echo + echo-xsi (an XSI compliant echo, both behaviors are different, cf. Standard)
- [ ] expand
- [ ] expr
- [x] false
- [ ] file
- [ ] find
- [ ] fold
- [ ] fuser
- [ ] gencat
- [ ] getconf
- [ ] grep
- [ ] head
- [ ] iconv
- [ ] id
- [ ] ipcrm
- [ ] ipcs
- [ ] join
- [x] kill
- [ ] link
- [x] ln
- [ ] logger
- [ ] logname
- [ ] lp
- [ ] ls
- [ ] m4
- [ ] mailx
- [ ] man
- [ ] mesg
- [ ] mkdir
- [x] mkfifo
- [ ] more
- [ ] mv
- [ ] newgrp
- [ ] nice
- [ ] nl
- [ ] nohup
- [ ] od
- [ ] paste
- [ ] patch
- [ ] pathchk
- [ ] pr
- [ ] printf
- [ ] ps
- [ ] pwd
- [ ] renice
- [x] rm
- [ ] rmdir
- [ ] sed
- [ ] sh Using [zsh](https://github.com/zsh-users/zsh) as a replacement
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
- [x] true
- [ ] tsort
- [x] tty
- [ ] type
- [ ] ulimit
- [x] uname
- [ ] unexpand
- [x] uniq
- [x] unlink
- [ ] vi Using [vim](https://github.com/vim/vim.git) as a replacement
- [ ] wc
- [ ] who
- [ ] write
- [ ] xargs

