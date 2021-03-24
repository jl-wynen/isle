#include "hubbardFermiAction.hpp"

#include "../core.hpp"
#include "../profile.hpp"
#include "../logging/logging.hpp"

namespace isle {
    namespace action {
        namespace {
            constexpr std::complex<double> I{0.0, 1.0};

            /// Calculate force w/o -i using the DIRECT_SINGLE algorithm for either particles or holes.
            /*
             * Constructs all partial A^-1 to the left of (1+A^-1)^-1 first ('left').
             * Constructs rest on the fly ('right', contains (1+A^-1)^-1).
             */

            template <typename HFM, typename KMatrix>
            CDVector forceDirectSinglePart(const HFM &hfm, const CDVector &phi,
                                           const KMatrix &k, const Species species) {
                ISLE_PROFILE_NVTX_RANGE(species == Species::PARTICLE
                                        ? "action::forceDirectSinglePart(particle)"
                                        : "action::forceDirectSinglePart(hole)");
                const auto nx = hfm.nx();
                const auto nt = getNt(phi, nx);

                if (nt < 2)
                    throw std::invalid_argument("nt < 2 in HubbardFermiAction algorithm DIRECT_SINGLE not supported");

                // build A^-1 and partial products on the left of (1+A^-1)^-1
                std::vector<CDMatrix> lefts;  // in reverse order
                lefts.reserve(nt-1);  // not storing full A^-1 here

#ifdef USE_CUDA
                // CUDA version does NOT support k != id

                ISLE_PROFILE_NVTX_PUSH("action::forceDirectSinglePart[lefts]");
                // first term for tau = nt-2
                auto f = hfm.F(nt-1, phi, species, true);
                lefts.emplace_back(mult_CDMatrix_wrapper(f,k,nx));
                // other terms
                for (std::size_t t = nt-2; t != 0; --t) {
                    hfm.F(f, t, phi, species, true);
                    lefts.emplace_back(mult_CDMatrix_wrapper(mult_CDMatrix_wrapper(f, k, nx),lefts.back(),nx)); // CUDA product CDMatrix * CDMatrix
                }
                // full A^-1
                hfm.F(f, 0, phi, species, true);
                const CDMatrix Ainv = mult_CDMatrix_wrapper(mult_CDMatrix_wrapper(f, k, nx), lefts.back(), nx); // CUDA product CDMatrix * CDMatrix
                ISLE_PROFILE_NVTX_POP();

                ISLE_PROFILE_NVTX_PUSH("action::forceDirectSinglePart[rights]");
                // start right with (1+A^-1)^-1
                CDMatrix AinvPlusId = IdMatrix<std::complex<double>>(nx) + Ainv; // CUDA? sum id + CDMatrix
                auto ipiv = std::make_unique<int[]>(AinvPlusId.rows());
                lu_CDMatrix_wrapper(AinvPlusId, ipiv, nx); // CUDA inplace LU-decomposition of CDMatrix

                CDMatrix right = trans(Ainv);
                CDVector force(nx*nt);  // the result

                // first term, tau = nt-1
                inv_CDMatrix_wrapper(AinvPlusId, right, ipiv, nx, true); // CUDA AinvPlusId^T^-1 * right
                spacevec(force, nt-1, nx) = blaze::diagonal(right);

                // all sites except tau = nt-1
                for (std::size_t tau = 0; tau < nt-1; ++tau) {
                    hfm.F(f, tau, phi, species, true);
                    if(tau){
                        right = mult_CDMatrix_wrapper(right, mult_CDMatrix_wrapper(f, k, nx), nx); // CUDA product CDMatrix * CDMatrix
                    }else{
                        right = f;
                        inv_CDMatrix_wrapper(AinvPlusId, right, ipiv, nx, false); // CUDA AinvPlusId^-1 * right
                    }
                    spacevec(force, tau, nx) = blaze::diagonal(mult_CDMatrix_wrapper(lefts[nt-1-tau-1], right, nx)); // CUDA product CDMatrix * CDMatrix
                }
#else // USE_CUDA
                ISLE_PROFILE_NVTX_PUSH("action::forceDirectSinglePart[lefts]");
                // first term for tau = nt-2
                auto f = hfm.F(nt-1, phi, species, true);
                lefts.emplace_back(f*k);
                // other terms
                for (std::size_t t = nt-2; t != 0; --t) {
                    hfm.F(f, t, phi, species, true);
                    lefts.emplace_back(f*k*lefts.back());
                }
                // full A^-1
                hfm.F(f, 0, phi, species, true);
                const CDMatrix Ainv = f * k * lefts.back();
                ISLE_PROFILE_NVTX_POP();

                ISLE_PROFILE_NVTX_PUSH("action::forceDirectSinglePart[rights]");
                // start right with (1+A^-1)^-1
                CDMatrix right = IdMatrix<std::complex<double>>(nx) + Ainv;
                auto ipiv = std::make_unique<int[]>(right.rows());
                invert(right, ipiv);

                CDVector force(nx*nt);  // the result

                // first term, tau = nt-1
                spacevec(force, nt-1, nx) = blaze::diagonal(Ainv*right);

                // all sites except tau = nt-1
                for (std::size_t tau = 0; tau < nt-1; ++tau) {
                    hfm.F(f, tau, phi, species, true);
                    right = right * f * k;
                    spacevec(force, tau, nx) = blaze::diagonal(lefts[nt-1-tau-1]*right);
                }
#endif // USE_CUDA
                ISLE_PROFILE_NVTX_POP();
                return force;
            }

            /// Calculate force using the DIRECT_SQUARE algorithm for DIA discretization.
            CDVector forceDirectSquare(const HubbardFermiMatrixDia &hfm,
                                       const CDVector &phi) {
                const auto nx = hfm.nx();
                const auto nt = getNt(phi, nx);

                // invert Q
                CDMatrix QInv{hfm.Q(phi)};
                auto ipiv = std::make_unique<int[]>(QInv.rows());
                invert(QInv, ipiv);

                // calculate force
                CDVector force(QInv.rows());
                decltype(hfm.Tplus(0ul, phi)) T;  // sparse or dense matrix
                for (std::size_t tau = 0; tau < nt; ++tau) {
                    hfm.Tplus(T, loopIdx(tau+1, nt), phi);
                    spacevec(force, tau, nx) = I*blaze::diagonal(T*spacemat(QInv, tau, loopIdx(tau+1, nt), nx));
                    hfm.Tminus(T, tau, phi);
                    spacevec(force, tau, nx) -= I*blaze::diagonal(spacemat(QInv, loopIdx(tau+1, nt), tau, nx)*T);
                }

                return force;
            }

            /// Calculate force using the DIRECT_SQUARE algorithm for EXP discretization.
            CDVector forceDirectSquare(const HubbardFermiMatrixExp &hfm,
                                       const CDVector &phi) {
                const auto nx = hfm.nx();
                const auto nt = getNt(phi, nx);

                // invert Q
                CDMatrix QInv{hfm.Q(phi)};
                auto ipiv = std::make_unique<int[]>(QInv.rows());
                invert(QInv, ipiv);

                // calculate force
                CDVector force(QInv.rows());
                decltype(hfm.Tplus(0ul, phi)) T;  // sparse or dense matrix
                for (std::size_t tau = 0; tau < nt; ++tau) {
                    hfm.Tplus(T, loopIdx(tau+1, nt), phi);
                    spacevec(force, tau, nx) = I*blaze::diagonal(spacemat(QInv, tau, loopIdx(tau+1, nt), nx)*T);
                    hfm.Tminus(T, tau, phi);
                    spacevec(force, tau, nx) -= I*blaze::diagonal(T*spacemat(QInv, loopIdx(tau+1, nt), tau, nx));
                }

                return force;
            }

        }  // anonymous namespace


        namespace _internal {
            template <> bool _holeShortcutPossible<HFABasis::PARTICLE_HOLE>(
                const SparseMatrix<double> &hopping,
                const double muTilde,
                const std::int8_t sigmaKappa) {

                auto log = getLogger("HubbardFermiAction");

                if (!isBipartite(hopping)) {
                    log.info("Not using shortcut for hole determinant, "
                             "lattice is not bipartite.");
                    return false;
                }
                else if (muTilde != 0.0) {  // must be exactlz zero
                    log.info("Not using shortcut for hole determinant, "
                             "chemical potential is not zero.");
                    return false;
                }
                else if (sigmaKappa != +1) {
                    log.info("Not using shortcut for hole determinant, "
                             "sigmaKappa is not +1");
                    // If other params do not allow for the shortcut,
                    // we hit an earlier return.
                    log.info("The other parameters allow using the shortcut, "
                             "consider setting sigmaKappa to +1 or explicitly "
                             "forbidding the use of the shortcut.");
                    return false;
                }

                log.info("Using shortcut to calculate hole determinant from "
                         "particle determinant.");
                return true;
            }

            template <> bool _holeShortcutPossible<HFABasis::SPIN>(
                const SparseMatrix<double> &UNUSED(hopping),
                const double UNUSED(muTilde),
                const std::int8_t UNUSED(sigmaKappa)) {

                getLogger("HubbardFermiAction").info(
                    "Not using shortcut for hole determinant, "
                    "spin basis is not supported.");
                return false;
            }
        }

        template <> std::complex<double>
        HubbardFermiAction<HFAHopping::DIA, HFAAlgorithm::DIRECT_SINGLE, HFABasis::PARTICLE_HOLE>::eval(
            const CDVector &phi) const {

            if (_shortcutForHoles) {
                const auto ldp = logdetM(_hfm, phi, Species::PARTICLE);
                return -toFirstLogBranch(ldp + std::conj(ldp));
            }
            else {
                return -toFirstLogBranch(logdetM(_hfm, phi, Species::PARTICLE)
                                         + logdetM(_hfm, phi, Species::HOLE));
            }
        }
        template <> CDVector
        HubbardFermiAction<HFAHopping::DIA, HFAAlgorithm::DIRECT_SINGLE, HFABasis::PARTICLE_HOLE>::force(
            const CDVector &phi) const {

            if (_shortcutForHoles) {
                const auto fp = forceDirectSinglePart(_hfm, phi, _kp, Species::PARTICLE);
                return -I*(fp - blaze::conj(fp));
            }
            else {
                return -I*(forceDirectSinglePart(_hfm, phi, _kp, Species::PARTICLE)
                         - forceDirectSinglePart(_hfm, phi, _kh, Species::HOLE));
            }
        }

        template <> std::complex<double>
        HubbardFermiAction<HFAHopping::DIA, HFAAlgorithm::DIRECT_SINGLE, HFABasis::SPIN>::eval(
            const CDVector &phi) const {

            const CDVector aux = -I*phi;
            return -toFirstLogBranch(logdetM(_hfm, aux, Species::PARTICLE)
                                     + logdetM(_hfm, aux, Species::HOLE));
        }
        template <> CDVector
        HubbardFermiAction<HFAHopping::DIA, HFAAlgorithm::DIRECT_SINGLE, HFABasis::SPIN>::force(
            const CDVector &phi) const {

            const CDVector aux = -I*phi;
            return (forceDirectSinglePart(_hfm, aux, _kh, Species::HOLE)
                    - forceDirectSinglePart(_hfm, aux, _kp, Species::PARTICLE));
        }

        template <> std::complex<double>
        HubbardFermiAction<HFAHopping::DIA, HFAAlgorithm::DIRECT_SQUARE, HFABasis::PARTICLE_HOLE>::eval(
            const CDVector &phi) const {

            return -logdetQ(_hfm, phi);
        }
        template <> CDVector
        HubbardFermiAction<HFAHopping::DIA, HFAAlgorithm::DIRECT_SQUARE, HFABasis::PARTICLE_HOLE>::force(
            const CDVector &phi) const {

            return forceDirectSquare(_hfm, phi);
        }

        template <> std::complex<double>
        HubbardFermiAction<HFAHopping::DIA, HFAAlgorithm::DIRECT_SQUARE, HFABasis::SPIN>::eval(
            const CDVector &phi) const {

            return -logdetQ(_hfm, -I*phi);
        }
        template <> CDVector
        HubbardFermiAction<HFAHopping::DIA, HFAAlgorithm::DIRECT_SQUARE, HFABasis::SPIN>::force(
            const CDVector &phi) const {

            return -I*forceDirectSquare(_hfm, -I*phi);
        }


        template <> std::complex<double>
        HubbardFermiAction<HFAHopping::EXP, HFAAlgorithm::DIRECT_SINGLE, HFABasis::PARTICLE_HOLE>::eval(
            const CDVector &phi) const {

            if (_shortcutForHoles) {
                const auto ldp = logdetM(_hfm, phi, Species::PARTICLE);
                return -toFirstLogBranch(ldp + std::conj(ldp));
            }
            else {
                return -toFirstLogBranch(logdetM(_hfm, phi, Species::PARTICLE)
                                         + logdetM(_hfm, phi, Species::HOLE));
            }
        }
        template <> CDVector
        HubbardFermiAction<HFAHopping::EXP, HFAAlgorithm::DIRECT_SINGLE, HFABasis::PARTICLE_HOLE>::force(
            const CDVector &phi) const {

            if (_shortcutForHoles) {
                const auto fp = forceDirectSinglePart(_hfm, phi, _kp, Species::PARTICLE);
                return -I*(fp - blaze::conj(fp));
            }
            else {
                return -I*(forceDirectSinglePart(_hfm, phi, _kp, Species::PARTICLE)
                         - forceDirectSinglePart(_hfm, phi, _kh, Species::HOLE));
            }
        }

        template <> std::complex<double>
        HubbardFermiAction<HFAHopping::EXP, HFAAlgorithm::DIRECT_SINGLE, HFABasis::SPIN>::eval(
            const CDVector &phi) const {

            const CDVector aux = -I*phi;
            return -toFirstLogBranch(logdetM(_hfm, aux, Species::PARTICLE)
                                     + logdetM(_hfm, aux, Species::HOLE));
        }
        template <> CDVector
        HubbardFermiAction<HFAHopping::EXP, HFAAlgorithm::DIRECT_SINGLE, HFABasis::SPIN>::force(
            const CDVector &phi) const {

            const CDVector aux = -I*phi;
            return (forceDirectSinglePart(_hfm, aux, _kh, Species::HOLE)
                    - forceDirectSinglePart(_hfm, aux, _kp, Species::PARTICLE));
        }

        template <> std::complex<double>
        HubbardFermiAction<HFAHopping::EXP, HFAAlgorithm::DIRECT_SQUARE, HFABasis::PARTICLE_HOLE>::eval(
            const CDVector &phi) const {

            return -logdetQ(_hfm, phi);
        }
        template <> CDVector
        HubbardFermiAction<HFAHopping::EXP, HFAAlgorithm::DIRECT_SQUARE, HFABasis::PARTICLE_HOLE>::force(
            const CDVector &phi) const {

            return forceDirectSquare(_hfm, phi);
        }

        template <> std::complex<double>
        HubbardFermiAction<HFAHopping::EXP, HFAAlgorithm::DIRECT_SQUARE, HFABasis::SPIN>::eval(
            const CDVector &phi) const {

            return -logdetQ(_hfm, -I*phi);
        }
        template <> CDVector
        HubbardFermiAction<HFAHopping::EXP, HFAAlgorithm::DIRECT_SQUARE, HFABasis::SPIN>::force(
            const CDVector &phi) const {

            return -I*forceDirectSquare(_hfm, -I*phi);
        }

        // instantiate all the templates we need right here
        template class HubbardFermiAction<HFAHopping::DIA, HFAAlgorithm::DIRECT_SINGLE, HFABasis::PARTICLE_HOLE>;
        template class HubbardFermiAction<HFAHopping::DIA, HFAAlgorithm::DIRECT_SINGLE, HFABasis::SPIN>;
        template class HubbardFermiAction<HFAHopping::DIA, HFAAlgorithm::DIRECT_SQUARE, HFABasis::PARTICLE_HOLE>;
        template class HubbardFermiAction<HFAHopping::DIA, HFAAlgorithm::DIRECT_SQUARE, HFABasis::SPIN>;

        template class HubbardFermiAction<HFAHopping::EXP, HFAAlgorithm::DIRECT_SINGLE, HFABasis::PARTICLE_HOLE>;
        template class HubbardFermiAction<HFAHopping::EXP, HFAAlgorithm::DIRECT_SINGLE, HFABasis::SPIN>;
        template class HubbardFermiAction<HFAHopping::EXP, HFAAlgorithm::DIRECT_SQUARE, HFABasis::PARTICLE_HOLE>;
        template class HubbardFermiAction<HFAHopping::EXP, HFAAlgorithm::DIRECT_SQUARE, HFABasis::SPIN>;

    } // namespace action
}  // namespace isle
