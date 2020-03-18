r"""!\file
\ingroup meas

# One-point functions

We can use the bilinear operators with vacuum quantum numbers
described in the SpinSpinCorrelator documentation to compute one-point functions.

All the vacuum quantum number bilinears can be written as combinations of the
number operators,
\f{align}{
    \left\langle n^p_x \right\rangle
          & = \left\langle a_x^\dagger a_x \right\rangle
            = \left\langle 1-a_x a_x^\dagger \right\rangle
            = \left\langle 1-P_{xx} \right\rangle
    \\
    \left\langle n^h_x \right\rangle
          & = \left\langle b_x^\dagger b_x \right\rangle
            = \left\langle 1-b_x b_x^\dagger \right\rangle
            = \left\langle 1-H_{xx} \right\rangle
\f}
For example, the expected charge density operator
\f[
    \rho_x = 1-2S^0_x = n^p_x - n^h_x
\f]
(as we use positive particles), the total number operator
\f[
    n_x = n^p_x + n^h_x
\f]
and the z-component of spin is given by
\f[
    S^3_x = \frac{1}{2} \left( 1 - n_x \right)
\f]

We can transform into a different basis; by default the results are in the spatial basis.
"""


from logging import getLogger

import numpy as np
import h5py as h5

import isle
from .measurement import Measurement
from ..util import temporalRoller
from ..h5io import createH5Group

fields = ["np", "nh"]

#TODO: save / retrieve einsum paths.

class measurement(Measurement):
    r"""!
    \ingroup meas
    Tabulate one-point correlators.
    """

    def __init__(self, particleAllToAll, holeAllToAll, savePath, configSlice=(None, None, None), transform=None):
        super().__init__(savePath, configSlice)

        # The correlation functions encoded here are between bilinear operators.
        # Since the individual constituents are fermionic, the bilinear is bosonic.
        self.fermionic = False

        self.particle=particleAllToAll
        self.hole=holeAllToAll

        self.data = {k: [] for k in fields}

        self.transform = transform

        self._einsum_path = None

    def __call__(self, stage, itr):
        """!Record the spin-spin correlators."""

        P = self.particle(stage, itr)
        H = self.hole(stage, itr)

        nx = P.shape[0]
        nt = P.shape[1]

        d = np.eye(nx*nt).reshape(*P.shape) # A Kronecker delta

        log = getLogger(__name__)

        data={}
        data["np"] = d-P
        data["nh"] = d-H

        if self._einsum_path is None:
            if self.transform is None:
                # No need for the transformation, cut the cost:
                self._einsum_path, _ = np.einsum_path("xtxt->x", data['np'], optimize="optimal")
                log.info("Optimized Einsum path for time averaging.")
            else:
                # We'll time average and transform at once:
                self._einsum_path, _ = np.einsum_path("ax,xtxt->a", self.transform, data["np"], optimize="optimal")
                log.info("Optimized Einsum path for time averaging and unitary transformation.")

        if self.transform is None:
            for correlator in self.data:
                measurement = np.einsum("xtxt->x", data[correlator], optimize=self._einsum_path) / nt
                self.data[correlator].append(measurement)
        else:
            for correlator in self.data:
                measurement = np.einsum("ax,xtxt->a", self.transform, data[correlator], optimize=self._einsum_path) / nt
                self.data[correlator].append(measurement)


    def save(self, h5group):
        r"""!
        \param h5group Base HDF5 group. Data is stored in subgroup `h5group/self.savePath`.
        """
        subGroup = createH5Group(h5group, self.savePath)
        if self.transform is None:
            subGroup["transform"] = h5.Empty(dtype="complex")
        else:
            subGroup["transform"] = self.transform
        for field in self.data:
            subGroup[field] = self.data[field]

def read(h5group):
    r"""!
    \param h5group HDF5 group which contains the data of this measurement.

    \returns A dictionary of measurements and a basis transformation, \n
    {    \n
        `np`: \f$n^p_x\f$,                      \n
        `nh`: \f$n^h_x\f$,                      \n
        `onePoint-transform`: A transformation from position space to another space \n
        }
    where both \f$n^{p,h}_x\f$ have the shape [measurements, spatial dimension] as long as `onePoint-transform` is square.
    """

    try:
        data = {key: h5group[key][()] for key in h5group}

        if type(data['transform']) is h5.Empty:
            data['transform'] = None

        data['onePoint-transform'] = data['transform']
        del data['transform']

        return data
    except:
        raise KeyError(f"Problem reading onePointFunction measurements from {h5group.name}")

def complete(measurements):
    r"""!
    \param measurements a dictionary of measurements, like what is returned from read().

    Measurements of one-point functions \f$\rho_x\f$, \f$n_x\f$,
    and \f$S^3_x\f$ are built from measurements of \f$n^p_x\f$ and \f$n^h_x\f$
    using the identities above.

    This can be used with the following example codeblock

    ```python
        with h5.File('measurements.h5','r') as f:
            data = isle.meas.onePointFunctions.read(f["correlation_functions/one_point"])

        data = isle.meas.onePointFunctions.complete(data)
    ```

    \returns a dictionary with additional one-point functions, built from those already computed.
    """

    try:
        rest = dict()

        rest["rho"] = measurements["np"] - measurements["nh"]
        rest["N"]   = measurements["np"] + measurements["nh"]
        rest["S3"]  = 0.5 * ( 1 - rest["N"])

        return {**measurements, **rest}

    except KeyError:
        raise KeyError(f"Needed fields np, nh are not both available in {measurements.keys()}")

    except ValueError:
        raise ValueError("Particle and hole one-point measurements are incompatible shapes, {measurements['np'].shape} and {measurements['nh'].shape}, respectively.")
