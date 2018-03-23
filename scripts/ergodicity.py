import sys
import os
import contextlib
import yaml
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.colors import LogNorm

import core
core.prepare_module_import()
import cns

LATFILE = "two_sites.yml"

NT = 32   # number of time slices
NTR = 100  # number of trajectories
NMD = 3  # number of MD steps per trajectory
MDSTEP = 1/NMD  # size of MD steps

U = 2
BETA = 3
SIGMA_KAPPA = 1

UTILDE = U*BETA/NT

def main():
    np.random.seed(1)

    with open(str(core.SCRIPT_PATH/"../lattices"/LATFILE), "r") as yamlf:
        lat = yaml.safe_load(yamlf)
    kappa = lat.hopping() * (BETA / NT)

    ham = cns.Hamiltonian(cns.HubbardGaugeAction(UTILDE),
                          cns.HubbardFermiAction(kappa, 0, SIGMA_KAPPA))

    # initial state
    phi = cns.Vector(np.random.normal(0, np.sqrt(UTILDE), lat.nx()*NT)+0j)

    # TODO store intitial traj?
    cfgs = []
    phases = []

    nacc = 0
    oldS = None
    for i in range(NTR):
        pi = cns.Vector(np.random.normal(0, np.sqrt(UTILDE), len(phi))+0j)
        if oldS is None:  # have never evaluated an action before, do it now
            oldS = ham.eval(phi, pi)

        # do MD
        newPhi, newPi, newS = cns.leapfrog(phi, pi, ham, 1, NMD)

        # reproducibility check
        # repPhi, repPi, _ = cns.leapfrog(newPhi, newPi, ham, 1, NMD, -1)
        # if np.linalg.norm(repPhi-phi) > 1e-10:
        #     print("Repro check failed in traj {} with error in phi: {}".format(i, np.linalg.norm(repPhi-phi)))
        #     return
        # if np.linalg.norm(repPi-pi) > 1e-10:
        #     print("Repro check failed in traj {} with error in pi: {}".format(i, np.linalg.norm(repPi-pi)))
        #     return

        # check for imaginary parts
        if np.max(cns.imag(newPhi)) > 1e-14:
            print("phi has acquired an imaginary part: {}".format(newPhi))
            return
        if np.max(cns.imag(newPi)) > 1e-14:
            print("pi has acquired an imaginary part: {}".format(newPhi))
            return

        if np.exp(cns.real(oldS-newS)) > np.random.uniform(0, 1):
            print("accept: ", newS-oldS)
            oldS = newS
            phi = newPhi
            nacc += 1
        else:
            print("reject: ", newS-oldS)

        cfgs.append(phi)
        phases.append(cns.imag(newS))


    print("max phase: ", np.max(np.abs(phases)))

    print("acceptance rate: ", nacc/NTR)

    dets= list(map(
        lambda cfg:
        np.exp(cns.logdet(cns.Matrix(
            cns.HubbardFermiMatrix(kappa, cfg, 0, SIGMA_KAPPA).M(False))
        )), cfgs))
    detsReal, detsImag = np.array(dets).real, np.array(dets).imag

    plt.figure()
    plt.title(r"$\mathrm{det}(M)$")
#    plt.hist(detsReal, bins=32)
#    plt.hist2d(detsReal, detsImag, [-20.+ i*40./40 for i in range(40)], norm=LogNorm())
    plt.hist2d(detsReal, detsImag, bins=40, norm=LogNorm())
    plt.show()

if __name__ == "__main__":
    main()
