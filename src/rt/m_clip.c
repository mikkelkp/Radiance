/* Copyright (c) 1986 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  m_clip.c - routine for clipped (cut) objects.
 *
 *     3/17/86
 */

#include  "ray.h"

/*
 *  Clipping objects permit holes and sections to be taken out
 *  of other objects.  The method is simple:  
 *
 *  The argument is the clipped materials;
 *  the first is used to shade upon exit.
 */


m_clip(m, r)			/* clip objects from ray */
register OBJREC  *m;
register RAY  *r;
{
	OBJECT  cset[MAXSET+1], *modset;
	int  entering;
	register int  i;

	if ((modset = (OBJECT *)m->os) == NULL) {
		OBJECT  obj, mod;

		if (m->oargs.nsargs < 1 || m->oargs.nsargs > MAXSET)
			objerror(m, USER, "bad # arguments");
		obj = objndx(m);
		modset = (OBJECT *)malloc((m->oargs.nsargs+1)*sizeof(OBJECT));
		if (modset == NULL)
			error(SYSTEM, "out of memory in m_clip");
		modset[0] = 0;
		for (i = 0; i < m->oargs.nsargs; i++) {
			if (!strcmp(m->oargs.sarg[i], VOIDID))
				continue;
			if ((mod = lastmod(obj, m->oargs.sarg[i])) == OVOID) {
				sprintf(errmsg, "unknown modifier \"%s\"",
						m->oargs.sarg[i]);
				objerror(m, WARNING, errmsg);
				continue;
			}
			if (inset(modset, mod)) {
				objerror(m, WARNING, "duplicate modifier");
				continue;
			}
			insertelem(modset, mod);
		}
		m->os = (char *)modset;
	}
	if (r->clipset != NULL)
		setcopy(cset, r->clipset);
	else
		cset[0] = 0;

	entering = r->rod > 0.0;		/* entering clipped region? */

	for (i = modset[0]; i > 0; i--) {
		if (entering) {
			if (!inset(cset, modset[i])) {
				if (cset[0] >= MAXSET)
					error(INTERNAL, "set overflow in m_clip");
				insertelem(cset, modset[i]);
			}
		} else {
			if (inset(cset, modset[i]))
				deletelem(cset, modset[i]);
		}
	}
					/* compute ray value */
	r->newcset = cset;
	if (strcmp(m->oargs.sarg[0], VOIDID)) {
		int  inside = 0;
		register RAY  *rp;
					/* check for penetration */
		for (rp = r; rp->parent != NULL; rp = rp->parent)
			if (!(rp->rtype & RAYREFL) && rp->parent->ro != NULL
					&& inset(modset, rp->parent->ro->omod))
				if (rp->parent->rod > 0.0)
					inside++;
				else
					inside--;
		if (inside > 0) {	/* we just hit the object */
			flipsurface(r);
			return(rayshade(r, modifier(m->oargs.sarg[0])));
		}
	}
	raytrans(r);			/* else transfer ray */
	return(1);
}
