"""!
Measurement of total phi and norm of phi.
"""

from pathlib import Path
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.ticker import NullFormatter

from .common import newAxes
from ..util import binnedArray
from ..h5io import createH5Group

class TotalPhi:
    r"""!
    \ingroup meas
    Tabulate phi and mean value of phi^2.
    """

    def __init__(self):
        self.Phi = []
        self.phiSq = []

    def __call__(self, phi, inline=False, **kwargs):
        """!Record the total phi and mean value of phi^2."""
        self.Phi.append(np.sum(phi))
        self.phiSq.append(np.linalg.norm(phi)**2 / len(phi))

    def reportPhiSq(self, binsize, ax=None, fmt=""):
        r"""!
        Plot the <phi^2> against Monte Carlo time.
        \param binsize The acceptance rate is averaged over `binsize` trajectories.
        \param ax Matplotlib Axes to plot in. If `None`, a new one is created in a new figure.
        \param fmt Plot format passed to matplotlib. Can encode color, marker and line styles.
        \returns The Axes with the plot.
        """

        binned = binnedArray(self.phiSq, binsize)

        # make a new axes is needed
        doTightLayout = False
        if ax is None:
            fig, ax = newAxes(r"global mean of <$\phi^2$> = {:3.5f}+/-{:3.5f}".format(np.mean(binned),
                                                                                      np.std(binned)),
                              r"$N_{\mathrm{tr}}$", r"<$\phi^2$>($N_{\mathrm{tr}})$")
            doTightLayout = True

        # plot <phi^2>
        ax.plot(np.arange(0, len(self.phiSq), binsize), binned,
                fmt, label=r"$\langle\phi^2\rangle$($N_{\mathrm{tr}})$")
        ax.set_ylim(ymin=0)
        if doTightLayout:
            fig.tight_layout()

        return ax

    def report(self, ax=None):
        spacer = 0.05
        left, width = 0.1, 0.65
        bottom, height = 0.1, 0.8
        
        fig = plt.figure()
        nullfmt = NullFormatter()
        
        history = plt.axes([left, bottom, width, height])
        dist    = plt.axes([left+width+spacer, bottom, 1-left-width-2*spacer, height])
        
        history.set_title(r"Monte Carlo History of $\Phi$")
        history.set_xlabel(r"$N_{\mathrm{tr}}$")
        history.set_ylabel(r"$\Phi$")
        history.plot(np.arange(len(self.Phi)), np.real(self.Phi), color='green', alpha=0.75)
        
        ylimits = history.get_ylim()
        
        dist.set_title(r"PDF")
        dist.set_xlabel(r"Freq.")
        dist.hist(np.real(self.Phi), 50, normed=1, facecolor='green', alpha=0.75, orientation="horizontal")
        dist.yaxis.set_major_formatter(nullfmt)
        dist.set_ylim(ylimits)

        return fig

    def reportPhiHistogram(self, ax=None):
        r"""!
        Plot histogram of summed Phi.
        \param ax Matplotlib Axes to plot in. If `None`, a new one is created in a new figure.
        \param fmt Plot format passed to matplotlib. Can encode color, marker and line styles.
        \returns The Axes with the plot.
        """

        # make a new axes is needed
        doTightLayout = False
        if ax is None:
            fig, ax = newAxes("", r"$\Phi$", r"PDF")
            doTightLayout = True

        # the histogram of the data
        ax.hist(np.real(self.Phi), 50, normed=1, facecolor='green', alpha=0.75)

        ax.grid(True)
        if doTightLayout:
            fig.tight_layout()

        return ax

    def reportPhi(self, ax=None, fmt=""):
        r"""!
        Plot monte carlo history of summed Phi.
        \param ax Matplotlib Axes to plot in. If `None`, a new one is created in a new figure.
        \param fmt Plot format passed to matplotlib. Can encode color, marker and line styles.
        \returns The Axes with the plot.
        """

        # make a new axes is needed
        doTightLayout = False
        if ax is None:
            fig, ax = newAxes("", r"$N_{\mathrm{tr}}$", r"$\Phi$")
            doTightLayout = True

        ax.plot(np.arange(len(self.Phi)), np.real(self.Phi), fmt,
                label=r"\Phi($i_{\mathrm{tr}})$")

        ax.grid(True)
        if doTightLayout:
            fig.tight_layout()

        return ax

    def save(self, base, name):
        r"""!
        Write both Phi and phiSquared.
        \param base HDF5 group in which to store data.
        \param name Name of the subgroup ob base for this measurement.
        """
        group = createH5Group(base, name)
        group["Phi"] = self.Phi
        group["phiSquared"] = self.phiSq

    def read(self, group):
        r"""!
        Read Phi and phiSquared from a file.
        \param group HDF5 group which contains the data of this measurement.
        """
        self.Phi = group["Phi"][()]
        self.phiSq = group["phiSquared"][()]
