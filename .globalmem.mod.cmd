savedcmd_/home/zhaoxg/dev/cpps/drivers/globalmem.mod := printf '%s\n'   globalmem.o | awk '!x[$$0]++ { print("/home/zhaoxg/dev/cpps/drivers/"$$0) }' > /home/zhaoxg/dev/cpps/drivers/globalmem.mod
