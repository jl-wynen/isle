import cns

# Physics parameters
latticeFile = "one_site.yml"    # input lattice
nt = 8                          # number of time slices
U = 2                           # Hubbard parameter
beta = 5                        # Inverse temperature
mu = 0                          # Chemical potential
sigma_kappa = -1

delta = beta / nt
UTilde = U * delta
muTilde = mu * delta
name = "{}.nt{}.U{}.beta{}.mu{}".format(latticeFile.split(".")[0],nt,U,beta,mu)


lattice = cns.readLattice(latticeFile)
kappaTilde = lattice.hopping() * delta  # actually \tilde{kappa}

nx = lattice.nx()
spacetime = nx*nt

# Evolution / HMC Information
nTherm = 3000               # number of thermalization trajectories
nLeapfrogTherm = 12         # number of steps in the leapfrog at the beginning of thermalization
nLeapfrog = 3               # production leapfrog steps
nProduction = 10000         # number of production trajectories

hamiltonian = cns.Hamiltonian(cns.HubbardGaugeAction(UTilde),
                          cns.HubbardFermiAction(kappaTilde, muTilde, sigma_kappa))

thermalizer = cns.hmc.LinearStepLeapfrog(hamiltonian, (1, 1), (nLeapfrogTherm, nLeapfrog), nTherm-1)
proposer = cns.hmc.ConstStepLeapfrog(hamiltonian, 1, nLeapfrog)