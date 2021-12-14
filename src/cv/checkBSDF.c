#ifndef lint
static const char RCSid[] = "$Id: checkBSDF.c,v 2.1 2021/12/14 02:33:18 greg Exp $";
#endif
/*
 *  checkBSDF.c
 *
 *  Load BSDF XML file and check Helmholtz reciprocity
 */

#define _USE_MATH_DEFINES
#include <math.h>
#include "rtio.h"
#include "bsdf.h"
#include "bsdf_m.h"
#include "bsdf_t.h"

#define	F_IN_COLOR	0x1
#define	F_ISOTROPIC	0x2
#define F_MATRIX	0x4
#define F_TTREE		0x8

/* Figure out BSDF type (and optionally determine if in color) */
const char *
getBSDFtype(const SDData *bsdf, int *flags)
{
	const SDSpectralDF	*df = bsdf->tb;

	if (flags) *flags = 0;
	if (!df) df = bsdf->tf;
	if (!df) df = bsdf->rf;
	if (!df) df = bsdf->rb;
	if (!df) return "Pure_Lambertian";
	if (df->comp[0].func == &SDhandleMtx) {
		const SDMat	*m = (const SDMat *)df->comp[0].dist;
		if (flags) {
			*flags |= F_MATRIX;
			*flags |= F_IN_COLOR*(m->chroma != NULL);
		}
		switch (m->ninc) {
		case 145:
			return "Klems_Full";
		case  73:
			return "Klems_Half";
		case  41:
			return "Klems_Quarter";
		}
		return "Unknown_Matrix";
	}
	if (df->comp[0].func == &SDhandleTre) {
		const SDTre	*t = (const SDTre *)df->comp[0].dist;
		if (flags) {
			*flags |= F_TTREE;
			*flags |= F_IN_COLOR*(t->stc[1] != NULL);
		}
		switch (t->stc[0]->ndim) {
		case 4:
			return "Anisotropic_Tensor_Tree";
		case 3:
			if (flags) *flags |= F_ISOTROPIC;
			return "Isotropic_Tensor_Tree";
		}
		return "Unknown_Tensor_Tree";
	}
	return "Unknown";
}

/* Report details related to one hemisphere distribution */
void
detailComponent(const char *nm, const SDValue *lamb, const SDSpectralDF *df)
{
	printf("%s\t%4.1f %4.1f %4.1f\t\t", nm, 100.*lamb->cieY*lamb->spec.cx/lamb->spec.cy,
			100.*lamb->cieY,
			100.*lamb->cieY*(1.f - lamb->spec.cx - lamb->spec.cy)/lamb->spec.cy);
	if (df)
		printf("%5.1f%%\t\t%.2f deg\n", 100.*df->maxHemi,
				sqrt(df->minProjSA/M_PI)*(360./M_PI));
	else
		puts("0%\t\t180");
}

/* Report reciprocity errors for the given directions */
void
checkReciprocity(const char *nm, const int side1, const int side2,
			const SDData *bsdf, const int fl)
{
	const SDSpectralDF	*df = bsdf->tf;
	double			emin=FHUGE, emax=0, esum=0;
	int			ntested=0;
	int			ec;

	if (side1 == side2) {
		df = (side1 > 0) ? bsdf->rf : bsdf->rb;
		if (!df) goto nothing2do;
	} else if (!bsdf->tf | !bsdf->tb)
		goto nothing2do;

	if (fl & F_MATRIX) {			/* special case for matrix BSDF */
		const SDMat	*m = (const SDMat *)df->comp[0].dist;
		int		i = m->ninc;
		FVECT		vin, vout;
		double		rerr;
		SDValue		vrev;
		while (i--) {
		    int	o = m->nout;
		    if (!mBSDF_incvec(vin, m, i+.5))
			continue;
		    while (o--) {
			if (!mBSDF_outvec(vout, m, o+.5))
				continue;
			rerr = mBSDF_value(m, o, i);
			if (rerr <= FTINY)
				continue;
			if (SDreportError( SDevalBSDF(&vrev, vout, vin, bsdf), stderr))
				return;
			rerr = 100.*fabs(rerr - vrev.cieY)/rerr;
			if (rerr < emin) emin = rerr;
			if (rerr > emax) emax = rerr;
			esum += rerr;
			++ntested;
		    }
		}
	} else {
	}
	if (ntested) {
		printf("%s\t%.1f\t%.1f\t%.1f\n", nm, emin, esum/(double)ntested, emax);
		return;
	}
nothing2do:
	printf("%s\t0\t0\t0\n", nm);
}

/* Report on the given BSDF XML file */
int
checkXML(char *fname)
{
	int		flags;
	SDError		ec;
	SDData		myBSDF;
	char		*pth;

	printf("File: '%s'\n", fname);
	SDclearBSDF(&myBSDF, fname);
	pth = getpath(fname, getrlibpath(), R_OK);
	if (!pth) {
		fprintf(stderr, "Cannot find file '%s'\n", fname);
		return 0;
	}
	ec = SDloadFile(&myBSDF, pth);
	if (ec) goto err;
	printf("Manufacturer: '%s'\n", myBSDF.makr);
	printf("BSDF Name: '%s'\n", myBSDF.matn);
	printf("Dimensions (W x H x Thickness): %g x %g x %g cm\n", 100.*myBSDF.dim[0],
				100.*myBSDF.dim[1], 100.*myBSDF.dim[2]);
	printf("Type: %s\n", getBSDFtype(&myBSDF, &flags));
	printf("Color: %d\n", (flags & F_IN_COLOR) != 0);
	printf("Has Geometry: %d\n", (myBSDF.mgf != NULL));
	puts("Component\tLambertian XYZ %\tMax. Dir\tMin. Angle");
	detailComponent("Internal Refl", &myBSDF.rLambFront, myBSDF.rf);
	detailComponent("External Refl", &myBSDF.rLambBack, myBSDF.rb);
	detailComponent("Int->Ext Trans", &myBSDF.tLambFront, myBSDF.tf);
	detailComponent("Ext->Int Trans", &myBSDF.tLambBack, myBSDF.tb);
	puts("Component\tReciprocity Error (min/avg/max %)");
	checkReciprocity("Front Refl", 1, 1, &myBSDF, flags);
	checkReciprocity("Back Refl", -1, -1, &myBSDF, flags);
	checkReciprocity("Transmission", -1, 1, &myBSDF, flags);
	SDfreeBSDF(&myBSDF);
	return 1;
err:
	SDreportError(ec, stderr);
	SDfreeBSDF(&myBSDF);
	return 0;
}

int
main(int argc, char *argv[])
{
	int	i;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s bsdf.xml ..\n", argv[0]);
		return 1;
	}
	for (i = 1; i < argc; i++) {
		puts("=====================================================");
		if (!checkXML(argv[i]))
			return 1;
	}
	return 0;
}
