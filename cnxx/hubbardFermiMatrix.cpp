#include "hubbardFermiMatrix.hpp"

#include <memory>

using namespace std::complex_literals;

namespace {
    /// Resize a square matrix, throws away old elements.
    template <typename MT>
    void resizeMatrix(MT &mat, const std::size_t target) {
#ifndef NDEBUG
        if (mat.rows() != mat.columns())
            throw std::invalid_argument("Matrix is not square.");
#endif
        if (mat.rows() != target)
            mat.resize(target, target, false);
    }
}

namespace cnxx {
/*
 * -------------------------- HubbardFermiMatrix --------------------------
 */

    HubbardFermiMatrix::HubbardFermiMatrix(const DSparseMatrix &kappa,
                                           const double mu,
                                           const std::int8_t sigmaKappa)
        : _kappa{kappa}, _mu{mu}, _sigmaKappa{sigmaKappa}
    {
#ifndef NDEBUG
        if (kappa.rows() != kappa.columns())
            throw std::invalid_argument("Hopping matrix is not square.");
#endif
    }


    void HubbardFermiMatrix::K(DSparseMatrix &k, const PH ph) const {
        const std::size_t NX = nx();
        resizeMatrix(k, NX);

        if (ph == PH::PARTICLE)
            k = (1+_mu)*IdMatrix<double>(NX) - _kappa;
        else
            k = (1-_mu)*IdMatrix<double>(NX) - _sigmaKappa*_kappa;
    }

    DSparseMatrix HubbardFermiMatrix::K(const PH ph) const {
        DSparseMatrix k;
        K(k, ph);
        return k;
    }

    void HubbardFermiMatrix::F(CDSparseMatrix &f,
                               const std::size_t tp, const CDVector &phi,
                               const PH ph, const bool inv) const {
        const std::size_t NX = nx();
        const std::size_t NT = phi.size()/NX;
        const std::size_t tm1 = tp==0 ? NT-1 : tp-1;  // t' - 1
        resizeMatrix(f, NX);

        if ((inv && ph == PH::PARTICLE) || (ph == PH::HOLE && !inv))
            blaze::diagonal(f) = blaze::exp(-1.i*spacevec(phi, tm1, NX));
        else
            blaze::diagonal(f) = blaze::exp(1.i*spacevec(phi, tm1, NX));
    }

    CDSparseMatrix HubbardFermiMatrix::F(const std::size_t tp, const CDVector &phi,
                                         const PH ph, const bool inv) const {
        CDSparseMatrix f;
        F(f, tp, phi, ph, inv);
        return f;
    }

    void HubbardFermiMatrix::M(CDSparseMatrix &m,
                               const CDVector &phi,
                               const PH ph) const {
        const std::size_t NX = nx();
        const std::size_t NT = phi.size()/NX;
        resizeMatrix(m, NX*NT);

        // zeroth row
        const auto k = K(ph);
        spacemat(m, 0, 0, NX) = k;
        // explicit boundary condition
        auto f = F(0, phi, ph);
        spacemat(m, 0, NT-1, NX) = f;

        // other rows
        for (std::size_t tp = 1; tp < NT; ++tp) {
            F(f, tp, phi, ph);
            spacemat(m, tp, tp-1, NX) = -f;
            spacemat(m, tp, tp, NX) = k;
        }
    }

    CDSparseMatrix HubbardFermiMatrix::M(const CDVector &phi,
                                         const PH ph) const {
        CDSparseMatrix m;
        M(m, phi, ph);
        return m;
    }


    void HubbardFermiMatrix::P(DSparseMatrix &P) const {
        const std::size_t NX = nx();
        resizeMatrix(P, NX);
        P = (2-_mu*_mu)*IdMatrix<double>(NX)
            - (_sigmaKappa*(1+_mu) + 1 - _mu)*_kappa
            + _sigmaKappa*_kappa*_kappa;
    }

    DSparseMatrix HubbardFermiMatrix::P() const {
        DSparseMatrix p;
        P(p);
        return p;
    }

    void HubbardFermiMatrix::Tplus(CDSparseMatrix &T,
                                   const std::size_t tp,
                                   const CDVector &phi) const {
        const std::size_t NX = nx();
        const std::size_t NT = phi.size()/NX;
        const std::size_t tm1 = tp==0 ? NT-1 : tp-1;  // t' - 1
        const double antiPSign = tp==0 ? -1 : 1;   // encode anti-periodic BCs
        resizeMatrix(T, NX);

        T = _sigmaKappa*_kappa - (1-_mu)*IdMatrix<std::complex<double>>(NX);
        for (std::size_t xp = 0; xp < NX; ++xp)
            blaze::row(T, xp) *= antiPSign*std::exp(1.i*phi[spacetimeCoord(xp, tm1, NX, NT)]);
    }

    CDSparseMatrix HubbardFermiMatrix::Tplus(const std::size_t tp,
                                             const CDVector &phi) const {
        CDSparseMatrix T;
        Tplus(T, tp, phi);
        return T;
    }

    void HubbardFermiMatrix::Tminus(CDSparseMatrix &T,
                                    const std::size_t tp,
                                    const CDVector &phi) const {
        const std::size_t NX = nx();
        const std::size_t NT = phi.size()/NX;
        const double antiPSign = tp==NT-1 ? -1 : 1;  // encode anti-periodic BCs
        resizeMatrix(T, NX);

        T = _kappa - (1+_mu)*IdMatrix<std::complex<double>>(NX);
        for (std::size_t x = 0; x < NX; ++x)
            blaze::column(T, x) *= antiPSign*std::exp(-1.i*phi[spacetimeCoord(x, tp, NX, NT)]);
    }

    CDSparseMatrix HubbardFermiMatrix::Tminus(const std::size_t tp,
                                              const CDVector &phi) const {
        CDSparseMatrix T;
        Tminus(T, tp, phi);
        return T;
    }

    void HubbardFermiMatrix::Q(CDSparseMatrix &q, const CDVector &phi) const {
        const std::size_t NX = nx();
        const std::size_t NT = phi.size()/NX;
        resizeMatrix(q, NX*NT);

        const auto p = P();
        SparseMatrix<std::complex<double>> aux; // for both T^+ and T^-
        for (std::size_t tp = 0; tp < NT; ++tp) {
            spacemat(q, tp, tp, NX) = p;

            Tplus(aux, tp, phi);
            spacemat(q, tp, (tp+NT-1)%NT, NX) += aux;

            Tminus(aux, tp, phi);
            spacemat(q, tp, (tp+1)%NT, NX) += aux;
        }
    }

    CDSparseMatrix HubbardFermiMatrix::Q(const CDVector &phi) const {
        CDSparseMatrix q;
        Q(q, phi);
        return q;
    }


    void HubbardFermiMatrix::updateKappa(const SparseMatrix<double> &kappa) {
        _kappa = kappa;
    }

    void HubbardFermiMatrix::updateMu(const double mu) {
        _mu = mu;
    }

    const SparseMatrix<double> &HubbardFermiMatrix::kappa() const noexcept {
        return _kappa;
    }

    double HubbardFermiMatrix::mu() const noexcept {
        return _mu;
    }

    std::int8_t HubbardFermiMatrix::sigmaKappa() const noexcept {
        return _sigmaKappa;
    }

    std::size_t HubbardFermiMatrix::nx() const noexcept {
        return _kappa.rows();
    }

/*
 * -------------------------- QLU --------------------------
 */

    HubbardFermiMatrix::QLU::QLU(const std::size_t nt) {
        dinv.reserve(nt);
        if (nt > 1) {
            u.reserve(nt-1);
            l.reserve(nt-1);
            if (nt > 2) {
                v.reserve(nt-2);
                h.reserve(nt-2);
            }
        }
    }

    bool HubbardFermiMatrix::QLU::isConsistent() const {
        const std::size_t nt = dinv.size();
        if (nt == 0)
            return false;
        else {
            // check this for any nt > 0
            if (u.size() != nt-1 || l.size() != nt-1)
                return false;
            // this check only works for nt > 1
            if (nt > 1 && (v.size() != nt-2 || h.size() != nt-2))
                return false;
        }
        return true;
    }

    CDMatrix HubbardFermiMatrix::QLU::reconstruct() const {
        const std::size_t nt = dinv.size();
        if (nt < 2)
            throw std::domain_error("Reconstruction of LU called with nt < 2");
#ifndef NDEBUG
        if (!isConsistent())
            throw std::runtime_error("Components of LU not initialized properly");
#endif

        const std::size_t nx = dinv[0].rows();
        std::unique_ptr<int[]> ipiv = std::make_unique<int[]>(nx);// pivot indices for inversion
        CDMatrix aux;

        CDMatrix recon(nx*nt, nx*nt, 0);

        // zeroth row, all columns
        invert(aux=dinv[0], ipiv);
        spacemat(recon, 0, 0, nx) = aux;
        spacemat(recon, 0, 1, nx) = u[0];
        spacemat(recon, 0, nt-1, nx) = v[0];

        // rows 1 - nt-3 (all 'regular' ones), all columns
        for (std::size_t i = 1; i < nt-2; ++i) {
            invert(aux=dinv[i], ipiv);
            spacemat(recon, i, i, nx) = aux + l[i-1]*u[i-1];
            invert(aux=dinv[i-1], ipiv);
            spacemat(recon, i, i-1, nx) = l[i-1]*aux;
            spacemat(recon, i, i+1, nx) = u[i];
            spacemat(recon, i, nt-1, nx) = l[i-1]*v[i-1] + v[i];
        }

        // row nt-2, all columns
        invert(aux=dinv[nt-2], ipiv);
        spacemat(recon, nt-2, nt-2, nx) = aux + l[nt-3]*u[nt-3];
        invert(aux=dinv[nt-3], ipiv);
        spacemat(recon, nt-2, nt-3, nx) = l[nt-3]*aux;
        spacemat(recon, nt-2, nt-1, nx) = l[nt-3]*v[nt-3] + u[nt-2];

        // row nt-1, up to column nt-3
        invert(aux=dinv[0], ipiv);
        spacemat(recon, nt-1, 0, nx) = h[0]*aux;
        for (std::size_t i = 1; i < nt-2; ++i) {
            invert(aux=dinv[i], ipiv);
            spacemat(recon, nt-1, i, nx) = h[i-1]*u[i-1] + h[i]*aux;
        }

        // row nt-1, columns nt-2, nt-1
        invert(aux=dinv[nt-2], ipiv);
        spacemat(recon, nt-1, nt-2, nx) = h[nt-3]*u[nt-3] + l[nt-2]*aux;
        invert(aux=dinv[nt-1], ipiv);
        spacemat(recon, nt-1, nt-1, nx) = aux + l[nt-2]*u[nt-2];
        for (std::size_t i = 0; i < nt-2; ++i)
            spacemat(recon, nt-1, nt-1, nx) += h[i]*v[i];

        return recon;
    }

/*
 * -------------------------- free functions --------------------------
 */

    namespace {
        /// Special case LU decomposition of Q for nt == 1.
        HubbardFermiMatrix::QLU nt1QLU(const HubbardFermiMatrix &hfm,
                                       const CDVector &phi) {
            HubbardFermiMatrix::QLU lu{1};

            // construct d_0
            CDSparseMatrix T = hfm.Tplus(0, phi);
            lu.dinv.emplace_back(hfm.P() + T);
            hfm.Tminus(T, 0, phi);
            lu.dinv[0] += T;

            // invert d_0
            std::unique_ptr<int[]> ipiv = std::make_unique<int[]>(phi.size());
            invert(lu.dinv[0], ipiv);

            return lu;
        }

        /// Special case LU decomposition of Q for nt == 2.
        HubbardFermiMatrix::QLU nt2QLU(const HubbardFermiMatrix &hfm,
                                       const CDVector &phi) {
            const std::size_t nx = hfm.nx();
            constexpr std::size_t nt = 2;
            HubbardFermiMatrix::QLU lu{nt};

            const auto P = hfm.P();  // diagonal block P
            SparseMatrix<std::complex<double>> aux0, aux1; // T^+, T^-, and u
            std::unique_ptr<int[]> ipiv = std::make_unique<int[]>(nx);// pivot indices for inversion

            // d_0
            lu.dinv.emplace_back(P);
            invert(lu.dinv[0], ipiv);

            // l_0
            hfm.Tplus(aux0, 1, phi);
            hfm.Tminus(aux1, 1, phi);
            lu.l.emplace_back((aux0+aux1)*lu.dinv[0]);

            // u_0
            hfm.Tplus(aux0, 0, phi);
            hfm.Tminus(aux1, 0, phi);
            aux0 += aux1;  // now aux0 = u_0
            lu.u.emplace_back(aux0);

            // d_1
            lu.dinv.emplace_back(P - lu.l[0]*aux0);
            invert(lu.dinv[1], ipiv);

            return lu;
        }

        /// General case LU decomposition of Q for nt > 2.
        HubbardFermiMatrix::QLU generalQLU(const HubbardFermiMatrix &hfm,
                                           const CDVector &phi) {
            const std::size_t nx = hfm.nx();
            const std::size_t nt = phi.size()/nx;
            HubbardFermiMatrix::QLU lu{nt};

            const auto P = hfm.P();  // diagonal block P
            SparseMatrix<std::complex<double>> T;  // subdiagonal blocks T^+ and T^-

            SparseMatrix<std::complex<double>> u; // previous u, updated along the way
            // pivot indices for inversion
            std::unique_ptr<int[]> ipiv = std::make_unique<int[]>(nx);

            // starting components of d, u, l
            lu.dinv.emplace_back(P);
            invert(lu.dinv.back(), ipiv);
            hfm.Tminus(u, 0, phi);   // now u = lu.u[0]
            lu.u.emplace_back(u);
            hfm.Tplus(T, 1, phi);
            lu.l.emplace_back(T*lu.dinv.back());

            // v, h
            hfm.Tplus(T, 0, phi);
            lu.v.emplace_back(T);
            hfm.Tminus(T, nt-1, phi);
            lu.h.emplace_back(T*lu.dinv.back());

            // iterate for i in [1, nt-3], 'regular' part of d, u, l, v, h
            for (std::size_t i = 1; i < nt-2; ++i) {
                // here, u = lu.u[i-1]
                lu.dinv.emplace_back(P - lu.l[i-1]*u);
                invert(lu.dinv.back(), ipiv);

                hfm.Tplus(T, i+1, phi);
                lu.l.emplace_back(T*lu.dinv[i]);
                lu.h.emplace_back(-lu.h[i-1]*u*lu.dinv[i]);
                lu.v.emplace_back(-lu.l[i-1]*lu.v[i-1]);

                hfm.Tminus(u, i, phi);
                lu.u.emplace_back(u);  // now u = lu.u[i]
            }
            // from now on u is lu.u[nt-3]

            // additional 'regular' step for d
            lu.dinv.emplace_back(P - lu.l[nt-3]*u);
            invert(lu.dinv.back(), ipiv);

            // final components of u, l
            hfm.Tminus(T, nt-2, phi);
            lu.u.emplace_back(T - lu.l[nt-3]*lu.v[nt-3]);
            hfm.Tplus(T, nt-1, phi);
            lu.l.emplace_back((T - lu.h[nt-3]*u)*lu.dinv[nt-2]);

            // final component of d
            lu.dinv.emplace_back(P - lu.l[nt-2]*lu.u[nt-2]);
            for (std::size_t i = 0; i < nt-2; ++i)
                lu.dinv[nt-1] -= lu.h[i]*lu.v[i];
            invert(lu.dinv.back(), ipiv);

            return lu;
        }
    }

    HubbardFermiMatrix::QLU getQLU(const HubbardFermiMatrix &hfm,
                                   const CDVector &phi) {
        switch (phi.size()/hfm.nx()) {
        case 1:
            return nt1QLU(hfm, phi);
        case 2:
            return nt2QLU(hfm, phi);
        default:
            return generalQLU(hfm, phi);
        }
    }

    Vector<std::complex<double>> solveQ(const HubbardFermiMatrix &hfm,
                                        const CDVector &phi,
                                        const Vector<std::complex<double>> &rhs) {
        return solveQ(getQLU(hfm, phi), rhs);
    }

    Vector<std::complex<double>> solveQ(const HubbardFermiMatrix::QLU &lu,
                                        const Vector<std::complex<double>> &rhs) {
        const std::size_t nt = lu.dinv.size();
#ifndef NDEBUG
        if (!lu.isConsistent())
            throw std::runtime_error("Components of LU not initialized properly");
        if (rhs.size() != lu.dinv[0].rows()*nt)
            throw std::runtime_error("Right hand side does not have correct size (spacetime vector)");
#endif

        const std::size_t nx = lu.dinv[0].rows();

        // solve L*y = rhs
        Vector<std::complex<double>> y(nt*nx);
        spacevec(y, 0, nx) = spacevec(rhs, 0, nx);
        for (std::size_t i = 1; i < nt-1; ++i)
            spacevec(y, i, nx) = spacevec(rhs, i, nx) - lu.l[i-1]*spacevec(y, i-1, nx);
        if (nt > 1) {
            spacevec(y, nt-1, nx) = spacevec(rhs, nt-1, nx) - lu.l[nt-2]*spacevec(y, nt-2, nx);
            for (std::size_t j = 0; j < nt-2; ++j)
                spacevec(y, nt-1, nx) -= lu.h[j]*spacevec(y, j, nx);
        }

        // solve U*x = y
        Vector<std::complex<double>> x(nt*nx);
    
        spacevec(x, nt-1, nx) = lu.dinv[nt-1]*spacevec(y, nt-1, nx);
        if (nt > 1) {
            spacevec(x, nt-2, nx) = lu.dinv[nt-2]*(spacevec(y, nt-2, nx)
                                                   - lu.u[nt-2]*spacevec(x, nt-1, nx));
            // iterate i in [nt-3, 0]
            for (std::size_t i = nt-3; i != static_cast<std::size_t>(-1); --i)
                spacevec(x, i, nx) = lu.dinv[i]*(spacevec(y, i, nx)
                                                 - lu.u[i]*spacevec(x, i+1, nx)
                                                 - lu.v[i]*spacevec(x, nt-1, nx));
        }

        return x;
    }

    std::complex<double> logdetQ(const HubbardFermiMatrix &hfm,
                                 const CDVector &phi) {
        auto lu = getQLU(hfm, phi);
        return ilogdetQ(lu);
    }

    std::complex<double> logdetQ(const HubbardFermiMatrix::QLU &lu) {
#ifndef NDEBUG
        if (!lu.isConsistent())
            throw std::runtime_error("Components of LU not initialized properly");
#endif

        std::complex<double> ldet;
        // calculate logdet of diagonal blocks
        for (const auto &dinv : lu.dinv)
            ldet -= logdet(dinv);
        return toFirstLogBranch(ldet);
    }

    std::complex<double> ilogdetQ(HubbardFermiMatrix::QLU &lu) {
#ifndef NDEBUG
        if (!lu.isConsistent())
            throw std::runtime_error("Components of LU not initialized properly");
#endif

        std::complex<double> ldet;
        // calculate logdet of diagonal blocks
        for (auto &dinv : lu.dinv)
            ldet -= ilogdet(dinv);
        return toFirstLogBranch(ldet);
    }


    std::complex<double> logdetM(const HubbardFermiMatrix &hfm,
                                 const CDVector &phi, const PH ph) {
        if (hfm.mu() != 0)
            throw std::runtime_error("Called logdetM with mu != 0. This is not supported yet because the algorithm is unstable.");

        const auto NX = hfm.nx();
        const auto NT = phi.size()/NX;

        DMatrix kinv = hfm.K(ph);
        auto ipiv = std::make_unique<int[]>(kinv.rows());
        invert(kinv, ipiv);

        // first K*F pair
        auto f = hfm.F(0, phi, ph, false);
        Matrix<std::complex<double>> aux = kinv*f;
        // other pairs
        for (std::size_t t = 1; t < NT; ++t) {
            hfm.F(f, t, phi, ph, false);
            aux = aux*kinv*f;
        }        

        // use kinv for first term because we already have it, otherwise would need dense K
        // gives extra minus sign
        return toFirstLogBranch(
            -static_cast<double>(NT)*logdet(kinv)
            + logdet(blaze::evaluate(IdMatrix<std::complex<double>>(NX) + aux))
            );
    }

}  // namespace cnxx
