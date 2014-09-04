LOPT = -lsocket -Zdll -Zmt -Zcrtdll
COPT = -mprobe

rxsockvm.dll: rxsockvm.c
	gcc -o rxsockvm.dll rxsockvm.c rxsockvm.def ${COPT} ${LOPT}
