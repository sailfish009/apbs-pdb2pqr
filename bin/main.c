/* ///////////////////////////////////////////////////////////////////////////
/// APBS -- Adaptive Poisson-Boltzmann Solver
///
///  Nathan A. Baker (nbaker@wasabi.ucsd.edu)
///  Dept. of Chemistry and Biochemistry
///  Dept. of Mathematics, Scientific Computing Group
///  University of California, San Diego 
///
///  Additional contributing authors listed in the code documentation.
///
/// Copyright � 1999. The Regents of the University of California (Regents).
/// All Rights Reserved. 
/// 
/// Permission to use, copy, modify, and distribute this software and its
/// documentation for educational, research, and not-for-profit purposes,
/// without fee and without a signed licensing agreement, is hereby granted,
/// provided that the above copyright notice, this paragraph and the
/// following two paragraphs appear in all copies, modifications, and
/// distributions.
/// 
/// IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
/// SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST PROFITS,
/// ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF
/// REGENTS HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.  
/// 
/// REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
/// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
/// PARTICULAR PURPOSE.  THE SOFTWARE AND ACCOMPANYING DOCUMENTATION, IF
/// ANY, PROVIDED HEREUNDER IS PROVIDED "AS IS".  REGENTS HAS NO OBLIGATION
/// TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
/// MODIFICATIONS. 
///
/// rcsid="$Id$"
//////////////////////////////////////////////////////////////////////////// */

/* ///////////////////////////////////////////////////////////////////////////
// File:     main.c
//
// Purpose:  APBS ``front end" using formatted input files
//
// Arguments:    
//           This driver program represents a mish-mash of instructions for
//           calculating electrostatic potentials, as well as free energies of 
//           binding and solvation.  It is invoked as:
//
//               apbs apbs.in
//
//           where apbs.in is a formatted input file (see example).
//
// Notes:    Doesn't have parallel implementation yet
//
// Returns:  APBSRC (defined below) on failure, 0 otherwise
//
// Author:   Nathan Baker
//
// rcsid="$Id$"
/////////////////////////////////////////////////////////////////////////// */

#include "apbscfg.h"
#include "apbs/apbs.h"  
#include "apbs/nosh.h"  
#include "apbs/mgparm.h"  
#include "apbs/pbeparm.h"  
#include "apbs/femparm.h"  

#define APBSRC 13

VEMBED(rcsid="$Id$")

typedef struct AtomForce {
   double ibForce[3];
   double qfForce[3];
   double dbForce[3];
   double npForce[3];
} AtomForce;

int main(int argc, char **argv) {

    NOsh *nosh = VNULL;
    MGparm *mgparm = VNULL;
    PBEparm *pbeparm = VNULL;
    FEMparm *femparm = VNULL;
    Vmem *mem = VNULL;
    Vio *sock = VNULL;
    Vpmg *pmg[NOSH_MAXCALC];
    Vpmgp *pmgp[NOSH_MAXCALC];
    Vpbe *pbe[NOSH_MAXCALC];
    Valist *alist[NOSH_MAXMOL];
    char *input_path = VNULL;
    char outpath[VMAX_ARGLEN];
    double ionstr, iparm, sparm;
    int i, j, imgcalc, ifemcalc;
    /* These variables require some explaining... The energy double arrays
     * store energies from the various calculations.  The energy int array
     * stores a flag (0, 1, 2) that indicates whether no, total, or all
     * energies were evaluated for a particular calculation.  Likewise, the
     * force double arrays store forces from the various calcualtions.  The
     * force int array stores an integer which either says no calculation was
     * performed (0) or gives the number of entries in the force array for each
     * calculation */
    double qfEnergy[NOSH_MAXCALC], qmEnergy[NOSH_MAXCALC];
    double dielEnergy[NOSH_MAXCALC], totEnergy[NOSH_MAXCALC];
    AtomForce *atomForce[NOSH_MAXCALC];
    double ibForce[3], qfForce[3], dbForce[3], npForce[3], tenergy;
    int nenergy[NOSH_MAXCALC], nforce[NOSH_MAXCALC], bytesTotal, highWater;
    int calcid;

    /* Instructions: */
    char *header = "\n\n\
    ----------------------------------------------------------------------\n\
    Adaptive Poisson-Boltzmann Solver (APBS)\n\
    Version 0.1.5 (October 25, 2001)\n\
    \n\
    Nathan A. Baker (nbaker@wasabi.ucsd.edu)\n\
    Dept. of Chemistry and Biochemistry\n\
    Dept. of Mathematics, Scientific Computing Group\n\
    University of California, San Diego \n\n\
    Additional contributing authors listed in the code documentation.\n\n\
    Copyright (c) 1999-2001.\n\
    The Regents of the University of California (Regents).\n\
    All Rights Reserved.\n\n\
    Permission to use, copy, modify, and distribute this software and its\n\
    documentation for educational, research, and not-for-profit purposes,\n\
    without fee and without a signed licensing agreement, is hereby granted,\n\
    provided that the above copyright notice, this paragraph and the\n\
    following two paragraphs appear in all copies, modifications, and\n\
    distributions.\n\n\
    IN NO EVENT SHALL REGENTS OR THE AUTHORS BE LIABLE TO ANY PARTY FOR\n\
    DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES,\n\
    INCLUDING LOST PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS\n\
    DOCUMENTATION, EVEN IF REGENTS OR THE AUTHORS HAVE BEEN ADVISED OF THE\n\
    POSSIBILITY OF SUCH DAMAGE.\n\n\
    REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT\n\
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A\n\
    PARTICULAR PURPOSE.  THE SOFTWARE AND ACCOMPANYING DOCUMENTATION, IF\n\
    ANY, PROVIDED HEREUNDER IS PROVIDED \"AS IS\".  REGENTS HAS NO OBLIGATION\n\
    TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR\n\
    MODIFICATIONS.\n\
    ----------------------------------------------------------------------\n\
    \n\n";
    char *usage = "\n\n\
    ----------------------------------------------------------------------\n\
    This driver program calculates electrostatic potentials, energies,\n\
    and forces using both multigrid and finite element methods.\n\
    It is invoked as:\n\n\
      apbs apbs.in\n\n\
    where apbs.in is a formatted input file.\n\
    ----------------------------------------------------------------------\n\n";

    /* A bit of array/pointer initialization */
    mem = Vmem_ctor("MAIN");
    for (i=0; i<NOSH_MAXCALC; i++) {
        atomForce[i] = VNULL;
        nenergy[i] = 0;
        nforce[i] = 0;
    }

    /* *************** CHECK INVOCATION ******************* */
    Vio_start();
    Vnm_tstart(26, "APBS WALL CLOCK");
    Vnm_print(1, "%s", header);
    Vnm_print(1, "This executable compiled on %s at %s\n\n", __DATE__, 
      __TIME__);
    if (argc != 2) {
        Vnm_print(2,"%s\n", usage);
        return APBSRC;
    } 
    input_path = argv[1];

    /* *************** PARSE INPUT FILE ******************* */
    nosh = NOsh_ctor(0);
    sock = Vio_ctor("FILE", "ASC", VNULL, input_path, "r");
    Vnm_print(1, "main:  Parsing input file %s...\n", input_path);
    if (!NOsh_parse(nosh, sock)) {
        Vnm_print(2, "main:  Error while parsing input file.\n");
        return APBSRC;
    } else Vnm_print(1, "main:  Parsed input file.\n");
    Vio_dtor(&sock);

    /* *************** LOAD MOLECULES ******************* */
    Vnm_print(1, "main:  Got PQR paths for %d molecules\n", nosh->nmol);
    for (i=0; i<nosh->nmol; i++) {
        Vnm_print(1, "main:  Reading atom data from %s:\n", nosh->molpath[i]);
        alist[i] = Valist_ctor();
        if (Valist_readPQR(alist[i], "FILE", "ASC", VNULL, 
          nosh->molpath[i]) != 1) {
            Vnm_print(2, "main:  Fatal error while reading from %s\n",
              nosh->molpath[i]);
            return APBSRC;
        } else {
            Vnm_print(1, "main:    %d atoms\n", 
              Valist_getNumberAtoms(alist[i])); 
            Vnm_print(1, "main:    Centered at (%4.3e, %4.3e, %4.3e)\n",
              alist[i]->center[0], alist[i]->center[1], alist[i]->center[2]);
            Vnm_print(1, "main:    Net charge %4.3e\n", alist[i]->charge);
        }
    }

    /* *************** DO THE CALCULATIONS ******************* */
    Vnm_print(1, "main:  Preparing to run %d PBE calculations.\n",
      nosh->ncalc);
    imgcalc = 0;
    ifemcalc = 0;
    for (i=0; i<nosh->ncalc; i++) {
        Vnm_print(1, "main:  ----------------------------------------\n");
        /* Do MG calculation */
        if (nosh->calc[i].calctype == 0) {
            mgparm = nosh->calc[imgcalc].mgparm;
            pbeparm = nosh->calc[imgcalc].pbeparm;
            imgcalc++;
            /* Set up missing MG parameters */
            if (mgparm->setgrid == 0) {
                mgparm->grid[0] = mgparm->glen[0]/((double)(mgparm->dime[0]-1));
                mgparm->grid[1] = mgparm->glen[1]/((double)(mgparm->dime[1]-1));
                mgparm->grid[2] = mgparm->glen[2]/((double)(mgparm->dime[2]-1));
            }
            if (mgparm->setglen == 0) {
                mgparm->glen[0] = mgparm->grid[0]*((double)(mgparm->dime[0]-1));
                mgparm->glen[1] = mgparm->grid[1]*((double)(mgparm->dime[1]-1));
                mgparm->glen[2] = mgparm->grid[2]*((double)(mgparm->dime[2]-1));
            }
            if (mgparm->cmeth == 1) {
                mgparm->center[0] = (alist[mgparm->centmol-1])->center[0];
                mgparm->center[1] = (alist[mgparm->centmol-1])->center[1];
                mgparm->center[2] = (alist[mgparm->centmol-1])->center[2];
            }
            /* If we're a parallel calculation, update the grid center based on
             * the appropriate shifts */
            if (mgparm->type == 2) {
                mgparm->center[0] += mgparm->partOlapCenterShift[0];
                mgparm->center[1] += mgparm->partOlapCenterShift[1];
                mgparm->center[2] += mgparm->partOlapCenterShift[2];
            }
            /* Check the BC */
            if ((pbeparm->bcfl == 4) && (i == 0)) {
                Vnm_print(2, "main: Can't focus first calculation!\n");
                return APBSRC;
            }
            ionstr = 0.0;
            for (j=0; j<pbeparm->nion; j++) 
              ionstr += 0.5*(VSQR(pbeparm->ionq[j])*pbeparm->ionc[j]);
            /* Print MG parameters */
            Vnm_print(1, "main:  CALCULATION #%d: MULTIGRID\n", i+1);
            Vnm_print(1, "main:    Grid dimensions: %d x %d x %d\n", 
              mgparm->dime[0], mgparm->dime[1], mgparm->dime[2]);
            Vnm_print(1, "main:    Grid spacings: %4.3f x %4.3f x %4.3f\n", 
              mgparm->grid[0], mgparm->grid[1], mgparm->grid[2]);
            Vnm_print(1, "main:    Grid lengths: %4.3f x %4.3f x %4.3f\n", 
              mgparm->glen[0], mgparm->glen[1], mgparm->glen[2]);
            Vnm_print(1, "main:    Grid center: (%4.3f, %4.3f, %4.3f)\n", 
              mgparm->center[0], mgparm->center[1], mgparm->center[2]);
            Vnm_print(1, "main:    Multigrid levels: %d\n", mgparm->nlev);
            Vnm_print(1, "main:    Molecule ID: %d\n", pbeparm->molid);
            if (pbeparm->nonlin) Vnm_print(1, "main:    Nonlinear PBE\n");
            else Vnm_print(1, "main:    Linearized PBE\n");
            if (pbeparm->bcfl == 0) {
                Vnm_print(1, "main:    Zero boundary conditions\n"); 
            } else if (pbeparm->bcfl == 1) {
                Vnm_print(1, "main:    Single Debye-Huckel sphere boundary \
conditions\n"); 
            } else if (pbeparm->bcfl == 2) {
                Vnm_print(1, "main:    Multiple Debye-Huckel sphere boundary \
conditions\n"); 
            } else if (pbeparm->bcfl == 4) {
                Vnm_print(1, "main:    Boundary conditions from focusing\n"); 
            }
            Vnm_print(1, "main:    %d ion species (%4.3f M ionic strength):\n",
              pbeparm->nion, ionstr);
            for (j=0; j<pbeparm->nion; j++) {
                Vnm_print(1, "main:      %4.3f A-radius, %4.3f e-charge, \
%4.3f M concentration\n", pbeparm->ionr[j], pbeparm->ionq[j], pbeparm->ionc[j]);
            }
            Vnm_print(1, "main:    Solute dielectric: %4.3f\n", pbeparm->pdie);
            Vnm_print(1, "main:    Solvent dielectric: %4.3f\n", pbeparm->sdie);
            if (pbeparm->srfm == 0) {
                Vnm_print(1, "main:    Using \"molecular\" surface \
definition; no smoothing\n");
                Vnm_print(1, "main:    Solvent probe radius: %4.3f A\n", 
                  pbeparm->srad);
            } else if (pbeparm->srfm == 1) {
                Vnm_print(1, "main:    Using \"molecular\" surface definition;\
 harmonic average smoothing\n");
                Vnm_print(1, "main:    Solvent probe radius: %4.3f A\n", 
                  pbeparm->srad);
            } else if (pbeparm->srfm == 2) {
                Vnm_print(1, "main:    Using spline-based surface definition;\
 window = %4.3f\n",
                  pbeparm->swin);
            }
            Vnm_print(1, "main:    Temperature:  %4.3f K\n", pbeparm->temp);
            Vnm_print(1, "main:    Surface tension:  %4.3f kJ/mol/A^2\n",
              pbeparm->gamma);
            if (pbeparm->calcenergy == 1) 
              Vnm_print(1, "main:    Electrostatic energies will be calculated\n");
            if (pbeparm->calcforce == 1)              
              Vnm_print(1, "main:    Net solvent forces will be calculated \n");
            if (pbeparm->calcforce == 2)              
              Vnm_print(1, "main:    All-atom solvent forces will be calculated\n");
            if (pbeparm->writepot == 1) {
                if (pbeparm->writepotfmt == 0) 
                  Vnm_print(1, "main:    Potential to be written to %s.%s in DX format\n",
                    pbeparm->writepotstem, "dx");
                if (pbeparm->writepotfmt == 1) 
                  Vnm_print(1, "main:    Potential to be written to %s.%s in AVS format\n",
                    pbeparm->writepotstem, "ucd");
                if (pbeparm->writepotfmt == 2) 
                  Vnm_print(1, "main:    Potential to be written to %s.%s in UHBD format\n",
                    pbeparm->writepotstem, "grd");
            }
            if (pbeparm->writeacc == 1) {
                if (pbeparm->writeaccfmt == 0) 
                  Vnm_print(1, "main:    Accessibility to be written to %s.%s in DX format\n",
                    pbeparm->writeaccstem, "dx");
                if (pbeparm->writeaccfmt == 1) 
                  Vnm_print(1, "main:    Accessibility to be written to %s.%s in AVS format\n",
                    pbeparm->writeaccstem, "ucd");
                if (pbeparm->writeaccfmt == 2) 
                  Vnm_print(1, "main:    Accessibility to be written to %sd.%s in UHBD format\n",
                    pbeparm->writeaccstem, "grd");
            }

            /* *************** PROBLEM SETUP ******************* */
            Vnm_tstart(27, "Setup timer");
            Vnm_print(1,"main:    Setting up parameters and accessibility object...\n");
            if (pbeparm->srfm == 2) sparm = pbeparm->swin;
            else sparm = pbeparm->srad;
            if (pbeparm->nion > 0) iparm = pbeparm->ionr[0];
            else iparm = 0.0;
            for (j=0; j<pbeparm->nion; j++)
              ionstr += 0.5*(VSQR(pbeparm->ionq[j])*pbeparm->ionc[j]);

            pbe[i] = Vpbe_ctor(alist[pbeparm->molid-1], pbeparm->nion,
              pbeparm->ionc, pbeparm->ionr, pbeparm->ionq, pbeparm->temp, 
              pbeparm->pdie, pbeparm->sdie, sparm);
            Vnm_print(1,"main:    Setting up PDE...\n");
            pmgp[i] = Vpmgp_ctor(mgparm->dime[0], mgparm->dime[1], 
              mgparm->dime[2], mgparm->nlev, mgparm->grid[0], mgparm->grid[1],
              mgparm->grid[2], pbeparm->nonlin);
            pmgp[i]->bcfl = pbeparm->bcfl;
            pmgp[i]->xcent = mgparm->center[0];
            pmgp[i]->ycent = mgparm->center[1];
            pmgp[i]->zcent = mgparm->center[2];
            /* See if we're supposed to be focusing this calculation */
            if (pbeparm->bcfl == 4) {
                if (i == 0) {
                    Vnm_print(2, "main:  Can't focus first calculation!\n");
                    return APBSRC;
                } 
                pmg[i] = Vpmg_ctorFocus(pmgp[i], pbe[i], pmg[i-1],
                  pbeparm->calcenergy);
            } else {
                if (i>0) Vpmg_dtor(&(pmg[i-1]));
                pmg[i] = Vpmg_ctor(pmgp[i], pbe[i]);
            }
            /* Other garbage collection */
            if (i>0) {
                Vpmgp_dtor(&(pmgp[i-1]));
                Vpbe_dtor(&(pbe[i-1]));
            } 
            Vpmg_fillco(pmg[i], pbeparm->srfm, pbeparm->swin);
            Vnm_redirect(0);
            Vnm_tstop(27, "Setup timer");
            Vnm_redirect(1);
            bytesTotal = Vmem_bytesTotal();
            highWater = Vmem_highWaterTotal();
            Vnm_print(1, "main:    Current memory usage:  %4.3f MB total, %4.3f MB high water\n",
              (double)(bytesTotal)/(1024.*1024.),
              (double)(highWater)/(1024.*1024.));
            
            /* *************** PROBLEM SETUP ******************* */
            Vnm_tstart(28, "Solver timer");
            Vnm_print(1,"main:    Solving PDE (see io.mc* for details)...\n");
            Vpmg_solve(pmg[i]);
            Vnm_redirect(0);
            Vnm_tstop(27, "Solver timer");
            Vnm_redirect(1);

            /* *************** POST-PROCESSING ******************* */
            /* Write out energies */
            if (pbeparm->calcenergy == 1) {
                nenergy[i] = 1;
                totEnergy[i] =
                  Vpmg_energy(pmg[i],1);
                Vnm_print(1, "main:    Total electrostatic energy = %1.12E\
 kJ/mol\n", Vunit_kb*pbeparm->temp*(1e-3)*Vunit_Na*totEnergy[i]);
            } else if (pbeparm->calcenergy == 2) {
                nenergy[i] = 1;
                totEnergy[i] = Vpmg_energy(pmg[i],1);
                qfEnergy[i] = Vpmg_qfEnergy(pmg[i],1);
                qmEnergy[i] = Vpmg_qmEnergy(pmg[i],1);
                dielEnergy[i] = Vpmg_dielEnergy(pmg[i],1);
                Vnm_print(1, "main:    Total electrostatic energy = %1.12E\
 kJ/mol\n", 
                  Vunit_kb*pbeparm->temp*(1e-3)*Vunit_Na*totEnergy[i]);
                Vnm_print(1, "main:    Fixed charge energy = %g kJ/mol\n", 
                  Vunit_kb*pbeparm->temp*(1e-3)*Vunit_Na*qfEnergy[i]);
                Vnm_print(1, "main:    Mobile charge energy = %g kJ/mol\n", 
                  Vunit_kb*pbeparm->temp*(1e-3)*Vunit_Na*qmEnergy[i]);
                Vnm_print(1, "main:    Dielectric energy = %g kJ/mol\n", 
                  Vunit_kb*pbeparm->temp*(1e-3)*Vunit_Na*dielEnergy[i]);
            } else nenergy[i] = 0;
            /* Print forces */
            if (pbeparm->calcforce == 1) {
                nforce[i] = 1;
                atomForce[i] = (AtomForce *)Vmem_malloc(mem, 1,
                  sizeof(AtomForce));
                atomForce[i][0].qfForce[0] = 0;
                atomForce[i][0].qfForce[1] = 0;
                atomForce[i][0].qfForce[2] = 0;
                atomForce[i][0].ibForce[0] = 0;
                atomForce[i][0].ibForce[1] = 0;
                atomForce[i][0].ibForce[2] = 0;
                atomForce[i][0].dbForce[0] = 0;
                atomForce[i][0].dbForce[1] = 0;
                atomForce[i][0].dbForce[2] = 0;
                atomForce[i][0].npForce[0] = 0;
                atomForce[i][0].npForce[1] = 0;
                atomForce[i][0].npForce[2] = 0;
                for (j=0;j<Valist_getNumberAtoms(alist[pbeparm->molid-1]);j++) {
                    Vpmg_qfForce(pmg[i], qfForce, j);
                    Vpmg_ibForce(pmg[i], ibForce, j);
                    Vpmg_dbnpForce(pmg[i], dbForce, npForce, pbeparm->gamma, j);
                    atomForce[i][0].qfForce[0] += qfForce[0];
                    atomForce[i][0].qfForce[1] += qfForce[1];
                    atomForce[i][0].qfForce[2] += qfForce[2];
                    atomForce[i][0].ibForce[0] += ibForce[0];
                    atomForce[i][0].ibForce[1] += ibForce[1];
                    atomForce[i][0].ibForce[2] += ibForce[2];
                    atomForce[i][0].dbForce[0] += dbForce[0];
                    atomForce[i][0].dbForce[1] += dbForce[1];
                    atomForce[i][0].dbForce[2] += dbForce[2];
                    atomForce[i][0].npForce[0] += npForce[0];
                    atomForce[i][0].npForce[1] += npForce[1];
                    atomForce[i][0].npForce[2] += npForce[2];
                }
                Vnm_print(1, "main:    Net total force on molecule %d\n", 
                  pbeparm->molid);
                Vnm_print(1, "           = (%4.3e, %4.3e, %4.3e) kJ/(mol A)\n",
                  Vunit_kb*pbeparm->temp*(1e-3)*Vunit_Na*(
                    atomForce[i][0].qfForce[0] + atomForce[i][0].ibForce[0] +
                    atomForce[i][0].dbForce[0] + atomForce[i][0].npForce[0]),
                  Vunit_kb*pbeparm->temp*(1e-3)*Vunit_Na*(
                    atomForce[i][0].qfForce[1] + atomForce[i][0].ibForce[1] +
                    atomForce[i][0].dbForce[1] + atomForce[i][0].npForce[1]),
                  Vunit_kb*pbeparm->temp*(1e-3)*Vunit_Na*(
                    atomForce[i][0].qfForce[2] + atomForce[i][0].ibForce[2] +
                    atomForce[i][0].dbForce[2] + atomForce[i][0].npForce[2]));
                Vnm_print(1, "main:    Net fixed charge force on molecule %d\n",
                  pbeparm->molid);
                Vnm_print(1, "           = (%4.3e, %4.3e, %4.3e) kJ/(mol A)\n", 
                  Vunit_kb*pbeparm->temp*(1e-3)*Vunit_Na*atomForce[i][0].qfForce[0], 
                  Vunit_kb*pbeparm->temp*(1e-3)*Vunit_Na*atomForce[i][0].qfForce[1],
                  Vunit_kb*pbeparm->temp*(1e-3)*Vunit_Na*atomForce[i][0].qfForce[2]); 
                Vnm_print(1, "main:    Net ionic boundary force on molecule %d\n",
                  pbeparm->molid);
                Vnm_print(1, "           = (%4.3e, %4.3e, %4.3e) kJ/(mol A)\n", 
                  Vunit_kb*pbeparm->temp*(1e-3)*Vunit_Na*atomForce[i][0].ibForce[0], 
                  Vunit_kb*pbeparm->temp*(1e-3)*Vunit_Na*atomForce[i][0].ibForce[1], 
                  Vunit_kb*pbeparm->temp*(1e-3)*Vunit_Na*atomForce[i][0].ibForce[2]); 
                Vnm_print(1, "main:    Net dielectric boundary force on molecule %d\n",
                  pbeparm->molid);
                Vnm_print(1, "           = (%4.3e, %4.3e, %4.3e) kJ/(mol A)\n",
                  Vunit_kb*pbeparm->temp*(1e-3)*Vunit_Na*atomForce[i][0].dbForce[0], 
                  Vunit_kb*pbeparm->temp*(1e-3)*Vunit_Na*atomForce[i][0].dbForce[1],
                  Vunit_kb*pbeparm->temp*(1e-3)*Vunit_Na*atomForce[i][0].dbForce[2]); 
                Vnm_print(1, "main:    Net apolar force on molecule %d\n",
                  pbeparm->molid);
                Vnm_print(1, "           = (%4.3e, %4.3e, %4.3e) kJ/(mol A)\n",
                  Vunit_kb*pbeparm->temp*(1e-3)*Vunit_Na*atomForce[i][0].npForce[0], 
                  Vunit_kb*pbeparm->temp*(1e-3)*Vunit_Na*atomForce[i][0].npForce[1],
                  Vunit_kb*pbeparm->temp*(1e-3)*Vunit_Na*atomForce[i][0].npForce[2]); 
            } else if (pbeparm->calcforce == 2) {
                nforce[i] = Valist_getNumberAtoms(alist[pbeparm->molid-1]);
                atomForce[i] = (AtomForce *)Vmem_malloc(mem, nforce[i],
                  sizeof(AtomForce));
                atomForce[i][0].npForce[2] = 0;
                for (j=0;j<Valist_getNumberAtoms(alist[pbeparm->molid-1]);j++) {
                    Vpmg_qfForce(pmg[i], atomForce[i][j].qfForce, j);
                    Vpmg_ibForce(pmg[i], atomForce[i][j].ibForce, j);
                    Vpmg_dbnpForce(pmg[i], atomForce[i][j].dbForce,
                      atomForce[i][j].npForce, pbeparm->gamma, j);
                    Vnm_print(1, "main:    Total force on atom %d, molecule %d = (%4.3e, %4.3e, %4.3e) kJ/(mol A)\n",
                      j, pbeparm->molid,
                      Vunit_kb*pbeparm->temp*(1e-3)*Vunit_Na*(
                        atomForce[i][j].qfForce[0]+atomForce[i][j].ibForce[0]+ 
                        atomForce[i][j].dbForce[0]+atomForce[i][j].npForce[0]),
                      Vunit_kb*pbeparm->temp*(1e-3)*Vunit_Na*(
                        atomForce[i][j].qfForce[1]+atomForce[i][j].ibForce[1]+
                        atomForce[i][j].dbForce[1]+atomForce[i][j].npForce[1]),
                      Vunit_kb*pbeparm->temp*(1e-3)*Vunit_Na*(
                        atomForce[i][j].qfForce[2]+atomForce[i][j].ibForce[2]+
                        atomForce[i][j].dbForce[2]+atomForce[i][j].npForce[2]));
                    Vnm_print(1, "main:    Fixed charge force on atom %d, molecule %d = (%4.3e, %4.3e, %4.3e) kJ/mol/A\n",
                     j, pbeparm->molid,
                     Vunit_kb*pbeparm->temp*(1e-3)*Vunit_Na*atomForce[i][j].qfForce[0], 
                     Vunit_kb*pbeparm->temp*(1e-3)*Vunit_Na*atomForce[i][j].qfForce[1],
                     Vunit_kb*pbeparm->temp*(1e-3)*Vunit_Na*atomForce[i][j].qfForce[2]);
                    Vnm_print(1, "main:    Ionic boundary force on atom %d, molecule %d = (%4.3e, %4.3e, %4.3e) kJ/mol/A\n",
                     j, pbeparm->molid,
                     Vunit_kb*pbeparm->temp*(1e-3)*Vunit_Na*atomForce[i][j].ibForce[0], 
                     Vunit_kb*pbeparm->temp*(1e-3)*Vunit_Na*atomForce[i][j].ibForce[1],
                     Vunit_kb*pbeparm->temp*(1e-3)*Vunit_Na*atomForce[i][j].ibForce[2]);
                    Vnm_print(1, "main:    Dielectric boundary force on atom %d, molecule %d = (%4.3e, %4.3e, %4.3e) kJ/mol/A\n",
                     j, pbeparm->molid,
                     Vunit_kb*pbeparm->temp*(1e-3)*Vunit_Na*atomForce[i][j].dbForce[0], 
                     Vunit_kb*pbeparm->temp*(1e-3)*Vunit_Na*atomForce[i][j].dbForce[1],
                     Vunit_kb*pbeparm->temp*(1e-3)*Vunit_Na*atomForce[i][j].dbForce[2]);
                    Vnm_print(1, "main:    Apolar force on atom %d, molecule %d = (%4.3e, %4.3e, %4.3e) kJ/mol/A\n",
                      j, pbeparm->molid,
                     Vunit_kb*pbeparm->temp*(1e-3)*Vunit_Na*atomForce[i][j].npForce[0], 
                     Vunit_kb*pbeparm->temp*(1e-3)*Vunit_Na*atomForce[i][j].npForce[1],
                     Vunit_kb*pbeparm->temp*(1e-3)*Vunit_Na*atomForce[i][j].npForce[2]);
                }
            } else nforce[i] = 0;

            /* Write potential */
            if (pbeparm->writepot == 1) {
                /* In DX format */
                if (pbeparm->writepotfmt == 0) {
                    sprintf(outpath, "%s.%s", pbeparm->writepotstem, "dx");
                    Vnm_print(1, "main:    Writing potential in DX format to %s...\n", 
                      outpath);
                    Vpmg_writeDX(pmg[i], "FILE", "ASC", VNULL, outpath,
                      "POTENTIAL", pmg[i]->u);

                /* In AVS format */
                } else if (pbeparm->writepotfmt == 1) {
                    sprintf(outpath, "%s.%s", pbeparm->writepotstem, "ucd");
                    Vnm_print(2, "main:    Sorry, AVS format isn't supported for multigrid calculations yet!\n");
                /* In UHBD format */
                } else if (pbeparm->writepotfmt == 2) {
                    sprintf(outpath, "%s.%s", pbeparm->writepotstem, "grd");
                    Vnm_print(1, "main:    Writing potential in UHBD format to %s...\n", 
                      outpath);
                    Vpmg_writeUHBD(pmg[i], "FILE", "ASC", VNULL, outpath,
                      "POTENTIAL", pmg[i]->u);
                } else Vnm_print(2, "main:    Bogus potential file format (%d)!\n", 
                         pbeparm->writepotfmt);
            }
            
            /* Write accessibility */
            if (pbeparm->writeacc == 1) {
                /* In DX format */
                if (pbeparm->writeaccfmt == 0) {
                    sprintf(outpath, "%s.%s", pbeparm->writeaccstem, "dx");
                    Vnm_print(1, "main:    Writing accessibility in DX format to %s...\n", outpath);
                    Vpmg_fillAcc(pmg[i], pmg[i]->rwork, 3, 0.3);
                    Vpmg_writeDX(pmg[i], "FILE", "ASC", VNULL, outpath,
                      "ACCESSIBILITY", pmg[i]->rwork);

                /* In AVS format */
                } else if (pbeparm->writeaccfmt == 1) {
                    sprintf(outpath, "%s.%s", pbeparm->writeaccstem, "ucd");
                    Vnm_print(2, "main:    Sorry, AVS format isn't supported\
for multigrid calculations yet!\n");
                /* In UHBD format */
                } else if (pbeparm->writeaccfmt == 2) {
                    sprintf(outpath, "%s.%s", pbeparm->writeaccstem, "grd");
                    Vnm_print(1, "main:    Writing accessibility in UHBD\
 format to %s...\n", outpath);
                    Vpmg_fillAcc(pmg[i], pmg[i]->rwork, 3, 0.3);
                    Vpmg_writeUHBD(pmg[i], "FILE", "ASC", VNULL, outpath,
                      "ACCESSIBILITY", pmg[i]->rwork);
                } else Vnm_print(2, "main:    Bogus accessibility file format\
(%d)!\n", pbeparm->writeaccfmt);
            }

            fflush(stdout);
            fflush(stderr);


        /* Do FEM calculation */
        } else {
            Vnm_print(2, "main: FEM shell support not implemented yet\n");
            return APBSRC;
        }

          
    } 

    
    /* *************** HANDLE PRINT STATEMENTS ******************* */
    if (nosh->nprint > 0) {
        Vnm_print(1, "main:  ----------------------------------------\n");
        Vnm_print(1, "main:  PRINT STATEMENTS\n");
    }
    for (i=0; i<nosh->nprint; i++) {
        /* Print energy */
        if (nosh->printwhat[i] == 0) {
            Vnm_print(1, "main:  print energy %d ", nosh->printcalc[i][0]);
            for (j=1; j<nosh->printnarg[i]; j++) {
                if (nosh->printop[i][j-1] == 0) 
                  Vnm_print(1, "+ ", nosh->printcalc[i][j]);
                else if (nosh->printop[i][j-1] == 1) 
                  Vnm_print(1, "- ", nosh->printcalc[i][j]);
                else {
                    Vnm_print(2, "main:  Undefined PRINT operation!\n");
                    break;
                }
                Vnm_print(1, "%d ", nosh->printcalc[i][j]);
            }
            Vnm_print(1, "end\n");
            calcid = nosh->elec2calc[nosh->printcalc[i][0]-1];
            if (nosh->calc[calcid].pbeparm->calcenergy != 0) {
                tenergy = Vunit_kb * (1e-3) * Vunit_Na * 
                  nosh->calc[calcid].pbeparm->temp * totEnergy[calcid];
            } else {
                Vnm_print(2, "main:    Didn't calculate energy in Calculation \
#%d\n", 
                  calcid+1);
                break;
            }
            for (j=1; j<nosh->printnarg[i]; j++) {
                calcid = nosh->elec2calc[nosh->printcalc[i][j]-1];
                if (nosh->calc[calcid].pbeparm->calcenergy != 0) {
                    if (nosh->printop[i][j-1] == 0)
                      tenergy = tenergy + Vunit_kb * (1e-3) * Vunit_Na *
                        nosh->calc[calcid].pbeparm->temp * totEnergy[calcid];
                    else if (nosh->printop[i][j-1] == 1)
                      tenergy = tenergy - Vunit_kb * (1e-3) * Vunit_Na * 
                        nosh->calc[calcid].pbeparm->temp * totEnergy[calcid];
                } else {  
                    Vnm_print(2, "main:    Didn't calculate energy in Calculation #%d\n", 
                      calcid+1);
                    break;
                }
            }
            Vnm_print(1, "main:    Answer = %1.12E kJ/mol\n", tenergy);
        } else {
            Vnm_print(2, "main:  Undefined PRINT keyword!\n");
            break;
        }
    }
 

    /* *************** GARBAGE COLLECTION ******************* */
    Vnm_print(1, "main:  ----------------------------------------\n");
    Vnm_print(1, "main:  CLEANING UP AND SHUTTING DOWN...\n");
    for (i=0; i<nosh->ncalc; i++) {
        if (nforce[i] > 0) 
          Vmem_free(mem, nforce[i], sizeof(AtomForce), 
            (void **)&(atomForce[i])); 
    }
    for (i=0; i<nosh->nmol; i++) Valist_dtor(&(alist[i]));
    NOsh_dtor(&nosh);
    Vmem_dtor(&mem);

    Vnm_redirect(0);
    Vnm_tstop(26, "APBS WALL CLOCK");
    Vnm_redirect(1);

    Vnm_print(1, "\n\nThanks for using APBS!\n\n");

    return 0;

}
