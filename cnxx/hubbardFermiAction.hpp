/** \file
 * \brief Fermionic part of Hubbard action.
 */

#ifndef HUBBARD_FERMI_ACTION_HPP
#define HUBBARD_FERMI_ACTION_HPP

#include "action.hpp"
#include "hubbardFermiMatrix.hpp"

namespace cnxx {
    /// Fermion action for Hubbard model.
    /**
     * The action is
     \f[
     S_{\mathrm{HFA}} = - \log \det M(\phi, \tilde{\kappa}, \tilde{\mu}) M(-\phi, \sigma_{\tilde{\kappa}}\tilde{\kappa}, -\tilde{\mu}),
     \f]
     * see HubbardFermiMatrix for the definition of M.
     *
     * Both variants of the algorithm are implemented and can be chosen in the constructor.
     * The default is variant 1. See <TT>docs/algorithm/hubbardFermiAction.pdf</TT>
     * for description and derivation of the algorithms.
     *
     * \warning Only supports `nt > 2`.
     * \todo Compute and cache det(K) and K^-1 or compute action via A^-1 and invert that.
     */
    class HubbardFermiAction : Action {
    public:
        /// Copy in a HubbardFermiMatrix.
        explicit HubbardFermiAction(const HubbardFermiMatrix &hfm, const bool variant2=false)
            : _hfm{hfm}, _kp{hfm.K(PH::PARTICLE)},
              _kh{hfm.K(PH::HOLE)}, _variant2{variant2} { }

        /// Construct from individual parameters of HubbardFermiMatrix.
        HubbardFermiAction(const SparseMatrix<double> &kappa,
                           double mu, std::int8_t sigmaKappa, const bool variant2=false)
            : _hfm{kappa, mu, sigmaKappa}, _kp{_hfm.K(PH::PARTICLE)},
              _kh{_hfm.K(PH::HOLE)}, _variant2{variant2} { }

        HubbardFermiAction(const HubbardFermiAction &other) = default;
        HubbardFermiAction &operator=(const HubbardFermiAction &other) = default;
        HubbardFermiAction(HubbardFermiAction &&other) = default;
        HubbardFermiAction &operator=(HubbardFermiAction &&other) = default;
        ~HubbardFermiAction() override = default;

        /// Evaluate the %Action for given auxilliary field phi.
        std::complex<double> eval(const CDVector &phi) override;

        /// Calculate force for given auxilliary field phi.
        CDVector force(const CDVector &phi) override;

    private:
        const HubbardFermiMatrix _hfm;  ///< Stores all necessary parameters.
        const DSparseMatrix _kp;  ///< Matrix K for particles.
        const DSparseMatrix _kh;  ///< Matrix K for holes.
        const bool _variant2;  ///< Use variant2 of the algorithm?
    };
}  // namespace cnxx

#endif  // ndef HUBBARD_FERMI_ACTION_HPP
