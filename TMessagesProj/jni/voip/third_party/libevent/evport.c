/*
 * Submitted by David Pacheco (dp.spambait@gmail.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY SUN MICROSYSTEMS, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL SUN MICROSYSTEMS, INC. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

/*
 * Copyright (c) 2007 Sun Microsystems. All rights reserved.
 * Use is subject to license terms.
 */

#include "config.h"

#include <sys/time.h>
#include <assert.h>
#include <sys/queue.h>
#include <errno.h>
#include <poll.h>
#include <port.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#ifdef CHECK_INVARIANTS
#include <assert.h>
#endif

#include "event.h"
#include "event-internal.h"
#include "log.h"
#include "evsignal.h"

#define DEFAULT_NFDS	16

#define EVENTS_PER_GETN 8

struct fd_info {
	struct event* fdi_revt;
	struct event* fdi_wevt;
};

#define FDI_HAS_READ(fdi)  ((fdi)->fdi_revt != NULL)
#define FDI_HAS_WRITE(fdi) ((fdi)->fdi_wevt != NULL)
#define FDI_HAS_EVENTS(fdi) (FDI_HAS_READ(fdi) || FDI_HAS_WRITE(fdi))
#define FDI_TO_SYSEVENTS(fdi) (FDI_HAS_READ(fdi) ? POLLIN : 0) | \
    (FDI_HAS_WRITE(fdi) ? POLLOUT : 0)

struct evport_data {
	int 		ed_port;
	int		ed_nevents;
	struct fd_info *ed_fds;

	int ed_pending[EVENTS_PER_GETN];
};

static void*	evport_init	(struct event_base *);
static int 	evport_add	(void *, struct event *);
static int 	evport_del	(void *, struct event *);
static int 	evport_dispatch	(struct event_base *, void *, struct timeval *);
static void	evport_dealloc	(struct event_base *, void *);

const struct eventop evportops = {
	"evport",
	evport_init,
	evport_add,
	evport_del,
	evport_dispatch,
	evport_dealloc,
	1
};

static void*
evport_init(struct event_base *base)
{
	struct evport_data *evpd;
	int i;

	if (evutil_getenv("EVENT_NOEVPORT"))
		return (NULL);

	if (!(evpd = calloc(1, sizeof(struct evport_data))))
		return (NULL);

	if ((evpd->ed_port = port_create()) == -1) {
		free(evpd);
		return (NULL);
	}

	evpd->ed_fds = calloc(DEFAULT_NFDS, sizeof(struct fd_info));
	if (evpd->ed_fds == NULL) {
		close(evpd->ed_port);
		free(evpd);
		return (NULL);
	}
	evpd->ed_nevents = DEFAULT_NFDS;
	for (i = 0; i < EVENTS_PER_GETN; i++)
		evpd->ed_pending[i] = -1;

	evsignal_init(base);

	return (evpd);
}

#ifdef CHECK_INVARIANTS

static void
check_evportop(struct evport_data *evpd)
{
	assert(evpd);
	assert(evpd->ed_nevents > 0);
	assert(evpd->ed_port > 0);
	assert(evpd->ed_fds > 0);

	int i;
	for (i = 0; i < evpd->ed_nevents; ++i) {
		struct event 	*ev;
		struct fd_info 	*fdi;

		fdi = &evpd->ed_fds[i];
		if ((ev = fdi->fdi_revt) != NULL) {
			assert(ev->ev_fd == i);
		}
		if ((ev = fdi->fdi_wevt) != NULL) {
			assert(ev->ev_fd == i);
		}
	}
}

static void
check_event(port_event_t* pevt)
{

	assert(pevt->portev_source == PORT_SOURCE_FD);
	assert(pevt->portev_user == NULL);
}

#else
#define check_evportop(epop)
#define check_event(pevt)
#endif

static int
grow(struct evport_data *epdp, int factor)
{
	struct fd_info *tmp;
	int oldsize = epdp->ed_nevents;
	int newsize = factor * oldsize;
	assert(factor > 1);

	check_evportop(epdp);

	tmp = realloc(epdp->ed_fds, sizeof(struct fd_info) * newsize);
	if (NULL == tmp)
		return -1;
	epdp->ed_fds = tmp;
	memset((char*) (epdp->ed_fds + oldsize), 0,
	    (newsize - oldsize)*sizeof(struct fd_info));
	epdp->ed_nevents = newsize;

	check_evportop(epdp);

	return 0;
}

static int
reassociate(struct evport_data *epdp, struct fd_info *fdip, int fd)
{
	int sysevents = FDI_TO_SYSEVENTS(fdip);

	if (sysevents != 0) {
		if (port_associate(epdp->ed_port, PORT_SOURCE_FD,
				   fd, sysevents, NULL) == -1) {
			event_warn("port_associate");
			return (-1);
		}
	}

	check_evportop(epdp);

	return (0);
}

static int
evport_dispatch(struct event_base *base, void *arg, struct timeval *tv)
{
	int i, res;
	struct evport_data *epdp = arg;
	port_event_t pevtlist[EVENTS_PER_GETN];

	int nevents = 1;

	struct timespec ts;
	struct timespec *ts_p = NULL;
	if (tv != NULL) {
		ts.tv_sec = tv->tv_sec;
		ts.tv_nsec = tv->tv_usec * 1000;
		ts_p = &ts;
	}

	for (i = 0; i < EVENTS_PER_GETN; ++i) {
		struct fd_info *fdi = NULL;
		if (epdp->ed_pending[i] != -1) {
			fdi = &(epdp->ed_fds[epdp->ed_pending[i]]);
		}

		if (fdi != NULL && FDI_HAS_EVENTS(fdi)) {
			int fd = FDI_HAS_READ(fdi) ? fdi->fdi_revt->ev_fd :
			    fdi->fdi_wevt->ev_fd;
			reassociate(epdp, fdi, fd);
			epdp->ed_pending[i] = -1;
		}
	}

	if ((res = port_getn(epdp->ed_port, pevtlist, EVENTS_PER_GETN,
		    (unsigned int *) &nevents, ts_p)) == -1) {
		if (errno == EINTR || errno == EAGAIN) {
			evsignal_process(base);
			return (0);
		} else if (errno == ETIME) {
			if (nevents == 0)
				return (0);
		} else {
			event_warn("port_getn");
			return (-1);
		}
	} else if (base->sig.evsignal_caught) {
		evsignal_process(base);
	}

	event_debug(("%s: port_getn reports %d events", __func__, nevents));

	for (i = 0; i < nevents; ++i) {
		struct event *ev;
		struct fd_info *fdi;
		port_event_t *pevt = &pevtlist[i];
		int fd = (int) pevt->portev_object;

		check_evportop(epdp);
		check_event(pevt);
		epdp->ed_pending[i] = fd;

		res = 0;
		if (pevt->portev_events & POLLIN)
			res |= EV_READ;
		if (pevt->portev_events & POLLOUT)
			res |= EV_WRITE;

		if (pevt->portev_events & (POLLERR|POLLHUP|POLLNVAL))
			res |= EV_READ|EV_WRITE;

		assert(epdp->ed_nevents > fd);
		fdi = &(epdp->ed_fds[fd]);

		if ((res & EV_READ) && ((ev = fdi->fdi_revt) != NULL)) {
			event_active(ev, res, 1);
		}

		if ((res & EV_WRITE) && ((ev = fdi->fdi_wevt) != NULL)) {
			event_active(ev, res, 1);
		}
	}

	check_evportop(epdp);

	return (0);
}

static int
evport_add(void *arg, struct event *ev)
{
	struct evport_data *evpd = arg;
	struct fd_info *fdi;
	int factor;

	check_evportop(evpd);

	if (ev->ev_events & EV_SIGNAL)
		return (evsignal_add(ev));

	factor = 1;
	while (ev->ev_fd >= factor * evpd->ed_nevents)
		factor *= 2;

	if (factor > 1) {
		if (-1 == grow(evpd, factor)) {
			return (-1);
		}
	}

	fdi = &evpd->ed_fds[ev->ev_fd];
	if (ev->ev_events & EV_READ)
		fdi->fdi_revt = ev;
	if (ev->ev_events & EV_WRITE)
		fdi->fdi_wevt = ev;

	return reassociate(evpd, fdi, ev->ev_fd);
}

static int
evport_del(void *arg, struct event *ev)
{
	struct evport_data *evpd = arg;
	struct fd_info *fdi;
	int i;
	int associated = 1;

	check_evportop(evpd);

	if (ev->ev_events & EV_SIGNAL) {
		return (evsignal_del(ev));
	}

	if (evpd->ed_nevents < ev->ev_fd) {
		return (-1);
	}

	for (i = 0; i < EVENTS_PER_GETN; ++i) {
		if (evpd->ed_pending[i] == ev->ev_fd) {
			associated = 0;
			break;
		}
	}

	fdi = &evpd->ed_fds[ev->ev_fd];
	if (ev->ev_events & EV_READ)
		fdi->fdi_revt = NULL;
	if (ev->ev_events & EV_WRITE)
		fdi->fdi_wevt = NULL;

	if (associated) {
		if (!FDI_HAS_EVENTS(fdi) &&
		    port_dissociate(evpd->ed_port, PORT_SOURCE_FD,
		    ev->ev_fd) == -1) {

			if (errno != EBADFD) {
				event_warn("port_dissociate");
				return (-1);
			}
		} else {
			if (FDI_HAS_EVENTS(fdi)) {
				return (reassociate(evpd, fdi, ev->ev_fd));
			}
		}
	} else {
		if (fdi->fdi_revt == NULL && fdi->fdi_wevt == NULL) {
			evpd->ed_pending[i] = -1;
		}
	}
	return 0;
}

static void
evport_dealloc(struct event_base *base, void *arg)
{
	struct evport_data *evpd = arg;

	evsignal_dealloc(base);

	close(evpd->ed_port);

	if (evpd->ed_fds)
		free(evpd->ed_fds);
	free(evpd);
}
