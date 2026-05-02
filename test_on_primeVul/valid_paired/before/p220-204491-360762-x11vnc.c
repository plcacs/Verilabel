static int shm_create(XShmSegmentInfo *shm, XImage **ximg_ptr, int w, int h,
    char *name) {

	XImage *xim;
	static int reported_flip = 0;
	int db = 0;

	shm->shmid = -1;
	shm->shmaddr = (char *) -1;
	*ximg_ptr = NULL;

	if (nofb) {
		return 1;
	}

	X_LOCK;

	if (! using_shm || xform24to32 || raw_fb) {
		/* we only need the XImage created */
		xim = XCreateImage_wr(dpy, default_visual, depth, ZPixmap,
		    0, NULL, w, h, raw_fb ? 32 : BitmapPad(dpy), 0);

		X_UNLOCK;

		if (xim == NULL) {
			rfbErr("XCreateImage(%s) failed.\n", name);
			if (quiet) {
				fprintf(stderr, "XCreateImage(%s) failed.\n",
				    name);
			}
			return 0;
		}
		if (db) fprintf(stderr, "shm_create simple %d %d\t%p %s\n", w, h, (void *)xim, name);
		xim->data = (char *) malloc(xim->bytes_per_line * xim->height);
		if (xim->data == NULL) {
			rfbErr("XCreateImage(%s) data malloc failed.\n", name);
			if (quiet) {
				fprintf(stderr, "XCreateImage(%s) data malloc"
				    " failed.\n", name);
			}
			return 0;
		}
		if (flip_byte_order) {
			char *order = flip_ximage_byte_order(xim);
			if (! reported_flip && ! quiet) {
				rfbLog("Changing XImage byte order"
				    " to %s\n", order);
				reported_flip = 1;
			}
		}

		*ximg_ptr = xim;
		return 1;
	}

	if (! dpy) {
		X_UNLOCK;
		return 0;
	}

	xim = XShmCreateImage_wr(dpy, default_visual, depth, ZPixmap, NULL,
	    shm, w, h);

	if (xim == NULL) {
		rfbErr("XShmCreateImage(%s) failed.\n", name);
		if (quiet) {
			fprintf(stderr, "XShmCreateImage(%s) failed.\n", name);
		}
		X_UNLOCK;
		return 0;
	}

	*ximg_ptr = xim;

#if HAVE_XSHM
	shm->shmid = shmget(IPC_PRIVATE,
	    xim->bytes_per_line * xim->height, IPC_CREAT | 0777);

	if (shm->shmid == -1) {
		rfbErr("shmget(%s) failed.\n", name);
		rfbLogPerror("shmget");

		XDestroyImage(xim);
		*ximg_ptr = NULL;

		X_UNLOCK;
		return 0;
	}

	shm->shmaddr = xim->data = (char *) shmat(shm->shmid, 0, 0);

	if (shm->shmaddr == (char *)-1) {
		rfbErr("shmat(%s) failed.\n", name);
		rfbLogPerror("shmat");

		XDestroyImage(xim);
		*ximg_ptr = NULL;

		shmctl(shm->shmid, IPC_RMID, 0);
		shm->shmid = -1;

		X_UNLOCK;
		return 0;
	}

	shm->readOnly = False;

	if (! XShmAttach_wr(dpy, shm)) {
		rfbErr("XShmAttach(%s) failed.\n", name);
		XDestroyImage(xim);
		*ximg_ptr = NULL;

		shmdt(shm->shmaddr);
		shm->shmaddr = (char *) -1;

		shmctl(shm->shmid, IPC_RMID, 0);
		shm->shmid = -1;

		X_UNLOCK;
		return 0;
	}
#endif

	X_UNLOCK;
	return 1;
}