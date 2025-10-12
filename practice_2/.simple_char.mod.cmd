savedcmd_simple_char.mod := printf '%s\n'   simple_char.o | awk '!x[$$0]++ { print("./"$$0) }' > simple_char.mod
