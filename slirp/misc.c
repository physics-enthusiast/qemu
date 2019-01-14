/*
 * Copyright (c) 1995 Danny Gasparovski.
 *
 * Please read the file COPYRIGHT for the
 * terms and conditions of the copyright.
 */

#include "qemu/osdep.h"
#include "slirp.h"
#include "libslirp.h"
#include "qemu/error-report.h"
#include "qemu/main-loop.h"

#ifdef DEBUG
int slirp_debug = DBG_CALL|DBG_MISC|DBG_ERROR;
#endif

inline void
insque(void *a, void *b)
{
	register struct quehead *element = (struct quehead *) a;
	register struct quehead *head = (struct quehead *) b;
	element->qh_link = head->qh_link;
	head->qh_link = (struct quehead *)element;
	element->qh_rlink = (struct quehead *)head;
	((struct quehead *)(element->qh_link))->qh_rlink
	= (struct quehead *)element;
}

inline void
remque(void *a)
{
  register struct quehead *element = (struct quehead *) a;
  ((struct quehead *)(element->qh_link))->qh_rlink = element->qh_rlink;
  ((struct quehead *)(element->qh_rlink))->qh_link = element->qh_link;
  element->qh_rlink = NULL;
}

int add_exec(struct ex_list **ex_ptr, void *chardev, const char *cmdline,
             struct in_addr addr, int port)
{
	struct ex_list *tmp_ptr;

	/* First, check if the port is "bound" */
	for (tmp_ptr = *ex_ptr; tmp_ptr; tmp_ptr = tmp_ptr->ex_next) {
		if (port == tmp_ptr->ex_fport &&
		    addr.s_addr == tmp_ptr->ex_addr.s_addr)
			return -1;
	}

	tmp_ptr = *ex_ptr;
	*ex_ptr = g_new0(struct ex_list, 1);
	(*ex_ptr)->ex_fport = port;
	(*ex_ptr)->ex_addr = addr;
	if (chardev) {
		(*ex_ptr)->ex_chardev = chardev;
	} else {
		(*ex_ptr)->ex_exec = g_strdup(cmdline);
	}
	(*ex_ptr)->ex_next = tmp_ptr;
	return 0;
}


#ifdef _WIN32

int
fork_exec(struct socket *so, const char *ex)
{
    /* not implemented */
    return 0;
}

#else

/*
 * XXX This is ugly
 * We create and bind a socket, then fork off to another
 * process, which connects to this socket, after which we
 * exec the wanted program.  If something (strange) happens,
 * the accept() call could block us forever.
 */
int
fork_exec(struct socket *so, const char *ex)
{
        int s, cs;
        struct sockaddr_in addr, csaddr;
	socklen_t addrlen = sizeof(addr);
        socklen_t csaddrlen = sizeof(csaddr);
	int opt;
	char **argv;
	int ret;
	pid_t pid;

	DEBUG_CALL("fork_exec");
	DEBUG_ARG("so = %p", so);
	DEBUG_ARG("ex = %p", ex);

    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    addr.sin_addr.s_addr = INADDR_ANY;

    s = qemu_socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0 || bind(s, (struct sockaddr *)&addr, addrlen) < 0 ||
        listen(s, 1) < 0) {
        error_report("Error: inet socket: %s", strerror(errno));
        if (s >= 0) {
            closesocket(s);
        }
        return 0;
    }

        if (getsockname(s, (struct sockaddr *)&csaddr, &csaddrlen) < 0) {
            closesocket(s);
            return 0;
        }
        cs = qemu_socket(AF_INET, SOCK_STREAM, 0);
        if (cs < 0) {
            closesocket(s);
            return 0;
        }
        csaddr.sin_addr = loopback_addr;
        /*
         * This connect won't block because we've already listen()ed on
         * the server end (even though we won't accept() the connection
         * until later on).
         */
        do {
            ret = connect(cs, (struct sockaddr *)&csaddr, csaddrlen);
        } while (ret < 0 && errno == EINTR);
        if (ret < 0) {
            closesocket(s);
            closesocket(cs);
            return 0;
        }

	pid = fork();
	switch(pid) {
	 case -1:
		error_report("Error: fork failed: %s", strerror(errno));
                closesocket(cs);
		close(s);
		return 0;

	 case 0:
                setsid();

		/* Set the DISPLAY */
                close(s);
                dup2(cs, 0);
                dup2(cs, 1);
                dup2(cs, 2);
		for (s = getdtablesize() - 1; s >= 3; s--)
		   close(s);

                argv = g_strsplit(ex, " ", -1);
		execvp(argv[0], (char **)argv);

		/* Ooops, failed, let's tell the user why */
        fprintf(stderr, "Error: execvp of %s failed: %s\n",
                argv[0], strerror(errno));
		close(0); close(1); close(2); /* XXX */
		exit(1);

	 default:
		qemu_add_child_watch(pid);
                closesocket(cs);
                /*
                 * This should never block, because we already connect()ed
                 * on the child end before we forked.
                 */
                do {
                    so->s = accept(s, (struct sockaddr *)&addr, &addrlen);
                } while (so->s < 0 && errno == EINTR);
                closesocket(s);
                socket_set_fast_reuse(so->s);
                opt = 1;
                qemu_setsockopt(so->s, SOL_SOCKET, SO_OOBINLINE, &opt, sizeof(int));
		qemu_set_nonblock(so->s);
		return 1;
	}
}
#endif

char *slirp_connection_info(Slirp *slirp)
{
    GString *str = g_string_new(NULL);
    const char * const tcpstates[] = {
        [TCPS_CLOSED]       = "CLOSED",
        [TCPS_LISTEN]       = "LISTEN",
        [TCPS_SYN_SENT]     = "SYN_SENT",
        [TCPS_SYN_RECEIVED] = "SYN_RCVD",
        [TCPS_ESTABLISHED]  = "ESTABLISHED",
        [TCPS_CLOSE_WAIT]   = "CLOSE_WAIT",
        [TCPS_FIN_WAIT_1]   = "FIN_WAIT_1",
        [TCPS_CLOSING]      = "CLOSING",
        [TCPS_LAST_ACK]     = "LAST_ACK",
        [TCPS_FIN_WAIT_2]   = "FIN_WAIT_2",
        [TCPS_TIME_WAIT]    = "TIME_WAIT",
    };
    struct in_addr dst_addr;
    struct sockaddr_in src;
    socklen_t src_len;
    uint16_t dst_port;
    struct socket *so;
    const char *state;
    char buf[20];

    g_string_append_printf(str,
        "  Protocol[State]    FD  Source Address  Port   "
        "Dest. Address  Port RecvQ SendQ\n");

    for (so = slirp->tcb.so_next; so != &slirp->tcb; so = so->so_next) {
        if (so->so_state & SS_HOSTFWD) {
            state = "HOST_FORWARD";
        } else if (so->so_tcpcb) {
            state = tcpstates[so->so_tcpcb->t_state];
        } else {
            state = "NONE";
        }
        if (so->so_state & (SS_HOSTFWD | SS_INCOMING)) {
            src_len = sizeof(src);
            getsockname(so->s, (struct sockaddr *)&src, &src_len);
            dst_addr = so->so_laddr;
            dst_port = so->so_lport;
        } else {
            src.sin_addr = so->so_laddr;
            src.sin_port = so->so_lport;
            dst_addr = so->so_faddr;
            dst_port = so->so_fport;
        }
        snprintf(buf, sizeof(buf), "  TCP[%s]", state);
        g_string_append_printf(str, "%-19s %3d %15s %5d ", buf, so->s,
                       src.sin_addr.s_addr ? inet_ntoa(src.sin_addr) : "*",
                       ntohs(src.sin_port));
        g_string_append_printf(str, "%15s %5d %5d %5d\n",
                       inet_ntoa(dst_addr), ntohs(dst_port),
                       so->so_rcv.sb_cc, so->so_snd.sb_cc);
    }

    for (so = slirp->udb.so_next; so != &slirp->udb; so = so->so_next) {
        if (so->so_state & SS_HOSTFWD) {
            snprintf(buf, sizeof(buf), "  UDP[HOST_FORWARD]");
            src_len = sizeof(src);
            getsockname(so->s, (struct sockaddr *)&src, &src_len);
            dst_addr = so->so_laddr;
            dst_port = so->so_lport;
        } else {
            snprintf(buf, sizeof(buf), "  UDP[%d sec]",
                         (so->so_expire - curtime) / 1000);
            src.sin_addr = so->so_laddr;
            src.sin_port = so->so_lport;
            dst_addr = so->so_faddr;
            dst_port = so->so_fport;
        }
        g_string_append_printf(str, "%-19s %3d %15s %5d ", buf, so->s,
                       src.sin_addr.s_addr ? inet_ntoa(src.sin_addr) : "*",
                       ntohs(src.sin_port));
        g_string_append_printf(str, "%15s %5d %5d %5d\n",
                       inet_ntoa(dst_addr), ntohs(dst_port),
                       so->so_rcv.sb_cc, so->so_snd.sb_cc);
    }

    for (so = slirp->icmp.so_next; so != &slirp->icmp; so = so->so_next) {
        snprintf(buf, sizeof(buf), "  ICMP[%d sec]",
                     (so->so_expire - curtime) / 1000);
        src.sin_addr = so->so_laddr;
        dst_addr = so->so_faddr;
        g_string_append_printf(str, "%-19s %3d %15s  -    ", buf, so->s,
                       src.sin_addr.s_addr ? inet_ntoa(src.sin_addr) : "*");
        g_string_append_printf(str, "%15s  -    %5d %5d\n", inet_ntoa(dst_addr),
                       so->so_rcv.sb_cc, so->so_snd.sb_cc);
    }

    return g_string_free(str, FALSE);
}
