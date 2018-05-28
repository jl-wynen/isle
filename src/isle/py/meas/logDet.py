"""!
Measurement of logdet.
"""

import numpy as np
from matplotlib.colors import LogNorm

import isle
from .common import newAxes
from ..h5io import createH5Group

class LogDet:
    r"""!
    \ingroup meas
    Measure the log determinant of particles and holes.
    """

    def __init__(self, kappaTilde, mu, SIGMA_KAPPA):
        self.hfm = isle.HubbardFermiMatrix(kappaTilde, mu, SIGMA_KAPPA)
        self.logdet = {isle.Species.PARTICLE: [], isle.Species.HOLE: []}

    def __call__(self, phi, inline=False, **kwargs):
        """!Record logdet."""
        for species in self.logdet:
            self.logdet[species].append(isle.logdetM(self.hfm, phi, species))

    def report(self, species=isle.Species.PARTICLE, binsize=41, ax=None):
        r"""!
        Plot the determinant in the complex plane
        \param species a `isle.Species` species that identifies the determinant of interest.
        \param binsize Number of bins in each direction.
                       An odd number highlights the avoidance of 0 determinant.
        \param ax Matplotlib Axes to plot in. If `None`, a new one is created in a new figure.
        \returns The Axes with the plot.
        """

        if ax is None:
            fig, ax = newAxes(str(species)+" Determinant", r"Re($det$)", r"Im($det$)")
            doTightLayout = True

        dets = np.exp(self.logdet[species])

        ax.hist2d(np.real(dets), np.imag(dets), bins=binsize, norm=LogNorm())
        ax.set_aspect('equal')

        if doTightLayout:
            fig.tight_layout()
        return ax

    def save(self, base, name):
        r"""!
        Write both the particle and hole logdet to a file.
        \param base HDF5 group in which to store data.
        \param name Name of the subgroup ob base for this measurement.
        """
        group = createH5Group(base, name)
        group["particles"] = self.logdet[isle.Species.PARTICLE]
        group["holes"] = self.logdet[isle.Species.HOLE]

    def read(self, group):
        r"""!
        Read particle and hole logdet from a file.
        \param group HDF5 group which contains the data of this measurement.
        """
        self.logdet[isle.Species.PARTICLE] = group["particles"][()]
        self.logdet[isle.Species.HOLE] = group["holes"][()]
