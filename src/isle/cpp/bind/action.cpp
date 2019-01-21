#include "action.hpp"

#include "../action/action.hpp"
#include "../action/hubbardGaugeAction.hpp"
#include "../action/hubbardFermiAction.hpp"
#include "../action/sumAction.hpp"

using namespace pybind11::literals;
using namespace isle;
using namespace isle::action;

namespace bind {
    namespace {
        /// Trampoline class for isle::action::Action to allow Python classes to
        /// override its virtual members.
        struct ActionTramp : Action {
            std::complex<double> eval(const Vector<std::complex<double>> &phi) const override {
                PYBIND11_OVERLOAD_PURE(
                    std::complex<double>,
                    Action,
                    eval,
                    phi
                );
            }

            Vector<std::complex<double>> force(
                const Vector<std::complex<double>> &phi) const override {

                PYBIND11_OVERLOAD_PURE(
                    Vector<std::complex<double>>,
                    Action,
                    force,
                    phi
                );
            }
        };

        void addAction(SumAction &sum, py::object &action) {
            try {
                // if action is a SumAction, add all its members
                SumAction *const otherSum = action.cast<SumAction*>();
                for (size_t i = 0; i < otherSum->size(); ++i)
                    sum.add((*otherSum)[i]);
            }
            catch (const py::cast_error &) {
                // if action is not a SumAction, just add it
                sum.add(action.cast<Action*>());
            }
        }

        auto bindBaseAction(py::module &mod) {
            return py::class_<Action, ActionTramp>(mod, "Action")
                .def(py::init<>())
                .def("eval", &Action::eval)
                .def("force", &Action::force)
                .def("__add__", [](py::object &self, py::object &other) {
                                    SumAction sum;
                                    addAction(sum, self);
                                    addAction(sum, other);
                                    return sum;
                                },
                    py::keep_alive<0, 1>(), py::keep_alive<0, 2>())
                ;
        }

        template <typename A>
        void bindSumAction(py::module &mod, A &action) {
            py::class_<SumAction>(mod, "SumAction", action)
                .def(py::init<>())
                .def(py::init([](py::args args) {
                                  SumAction act;
                                  for (auto arg : args)
                                      act.add(arg.cast<Action*>());
                                  return act;
                              }),
                    py::keep_alive<1, 2>())
                .def("add", [](SumAction &self, Action *const action) {
                                self.add(action);
                            },
                    py::keep_alive<1, 2>())
                // .def("__add__", [](SumAction *const this_, Action *const other) {
                //                     SumAction sum(*this_);
                //                     sum.add(other);
                //                     return sum;
                //                 },
                //     py::keep_alive<0, 1>(), py::keep_alive<0, 2>())
                .def("__getitem__", py::overload_cast<std::size_t>(&SumAction::operator[]),
                     py::return_value_policy::reference_internal)
                .def("__len__", &SumAction::size)
                .def("clear", &SumAction::clear)
                .def("eval", &SumAction::eval)
                .def("force", &SumAction::force)
                ;
        }

        template <typename a>
        void bindHubbardGaugeAction(py::module &mod, a &action) {
            py::class_<HubbardGaugeAction>(mod, "HubbardGaugeAction", action)
                .def(py::init<double>())
                .def("eval", &HubbardGaugeAction::eval)
                .def("force", &HubbardGaugeAction::force)
                ;
        }

        template <HFAHopping HOPPING, HFAVariant VARIANT, HFABasis BASIS,
                  typename A>
        void bindSpecificHFA(py::module &mod, const char * const name, A &action) {
            using HFA = HubbardFermiAction<HOPPING, VARIANT, BASIS>;

            py::class_<HFA>(mod, name, action)
                .def(py::init<SparseMatrix<double>, double, std::int8_t>(),
                     "kappa"_a, "mu"_a, "sigmaKappa"_a)
                .def("eval", &HFA::eval)
                .def("force", &HFA::force)
                ;
        }

        /// Make a specific HubbardFermiAction controlled through run-time parameters.
        py::object makeHubbardFermiAction(const SparseMatrix<double> &kappaTilde,
                                          const double muTilde,
                                          const std::int8_t sigmaKappa,
                                          const HFAHopping hopping,
                                          const HFABasis basis,
                                          const HFAVariant variant) {

            if (basis == HFABasis::PARTICLE_HOLE) {
                if (hopping == HFAHopping::DIA) {
                    if (variant == HFAVariant::ONE) {
                        return py::cast(HubbardFermiAction<HFAHopping::DIA,
                                        HFAVariant::ONE,
                                        HFABasis::PARTICLE_HOLE>(kappaTilde, muTilde, sigmaKappa));
                    } else {  // HFAVariant::TWO
                        return py::cast(HubbardFermiAction<HFAHopping::DIA,
                                        HFAVariant::TWO,
                                        HFABasis::PARTICLE_HOLE>(kappaTilde, muTilde, sigmaKappa));
                    }
                } else {  // HFAHopping::EXP
                    if (variant == HFAVariant::ONE) {
                        return py::cast(HubbardFermiAction<HFAHopping::EXP,
                                        HFAVariant::ONE,
                                        HFABasis::PARTICLE_HOLE>(kappaTilde, muTilde, sigmaKappa));
                    } else {  // HFAVariant::TWO
                        return py::cast(HubbardFermiAction<HFAHopping::EXP,
                                        HFAVariant::TWO,
                                        HFABasis::PARTICLE_HOLE>(kappaTilde, muTilde, sigmaKappa));
                    }
                }
            }

            else {  // HFABasis::SPIN
                if (hopping == HFAHopping::DIA) {
                    if (variant == HFAVariant::ONE) {
                        return py::cast(HubbardFermiAction<HFAHopping::DIA,
                                        HFAVariant::ONE,
                                        HFABasis::SPIN>(kappaTilde, muTilde, sigmaKappa));
                    } else {  // HFAVariant::TWO
                        return py::cast(HubbardFermiAction<HFAHopping::DIA,
                                        HFAVariant::TWO,
                                        HFABasis::SPIN>(kappaTilde, muTilde, sigmaKappa));
                    }
                } else {  // HFAHopping::EXP
                    if (variant == HFAVariant::ONE) {
                        return py::cast(HubbardFermiAction<HFAHopping::EXP,
                                        HFAVariant::ONE,
                                        HFABasis::SPIN>(kappaTilde, muTilde, sigmaKappa));
                    } else {  // HFAVariant::TWO
                        return py::cast(HubbardFermiAction<HFAHopping::EXP,
                                        HFAVariant::TWO,
                                        HFABasis::SPIN>(kappaTilde, muTilde, sigmaKappa));
                    }
                }
            }
        }

        /// Bind everything related to HubbardFermiActions.
        template <typename A>
        void bindHubbardFermiAction(py::module &mod, A &action) {
            // bind enums
            py::enum_<HFAVariant>(mod, "HFAVariant")
                .value("ONE", HFAVariant::ONE)
                .value("TWO", HFAVariant::TWO);

            py::enum_<HFABasis>(mod, "HFABasis")
                .value("PARTICLE_HOLE", HFABasis::PARTICLE_HOLE)
                .value("SPIN", HFABasis::SPIN);

            py::enum_<HFAHopping>(mod, "HFAHopping")
                .value("DIA", HFAHopping::DIA)
                .value("EXP", HFAHopping::EXP);

            // bind all specific actions
            bindSpecificHFA<HFAHopping::DIA, HFAVariant::ONE, HFABasis::PARTICLE_HOLE>(mod, "HubbardFermiActionDiaOneOne", action);
            bindSpecificHFA<HFAHopping::DIA, HFAVariant::ONE, HFABasis::SPIN>(mod, "HubbardFermiActionDiaOneZero", action);
            bindSpecificHFA<HFAHopping::DIA, HFAVariant::TWO, HFABasis::PARTICLE_HOLE>(mod, "HubbardFermiActionDiaTwoOne", action);
            bindSpecificHFA<HFAHopping::DIA, HFAVariant::TWO, HFABasis::SPIN>(mod, "HubbardFermiActionDiaTwoZero", action);

            bindSpecificHFA<HFAHopping::EXP, HFAVariant::ONE, HFABasis::PARTICLE_HOLE>(mod, "HubbardFermiActionExpOneOne", action);
            bindSpecificHFA<HFAHopping::EXP, HFAVariant::ONE, HFABasis::SPIN>(mod, "HubbardFermiActionExpOneZero", action);
            bindSpecificHFA<HFAHopping::EXP, HFAVariant::TWO, HFABasis::PARTICLE_HOLE>(mod, "HubbardFermiActionExpTwoOne", action);
            bindSpecificHFA<HFAHopping::EXP, HFAVariant::TWO, HFABasis::SPIN>(mod, "HubbardFermiActionExpTwoZero", action);

            mod.def("makeHubbardFermiAction",
                    makeHubbardFermiAction,
                    "kappaTilde"_a, "muTilde"_a, "sigmaKappa"_a,
                    "hopping"_a=HFAHopping::DIA,
                    "basis"_a=HFABasis::PARTICLE_HOLE,
                    "variant"_a= HFAVariant::ONE);

            mod.def("makeHubbardFermiAction",
                    [] (const Lattice &lattice, const double beta,
                        const double muTilde, const std::int8_t sigmaKappa,
                        const HFAHopping hopping, const HFABasis basis,
                        const HFAVariant variant) {

                        return makeHubbardFermiAction(
                            lattice.hopping()*beta/lattice.nt(),
                            muTilde, sigmaKappa,
                            hopping, basis, variant);
                    },
                    "lat"_a, "beta"_a, "muTilde"_a, "sigmaKappa"_a,
                    "hopping"_a=HFAHopping::DIA,
                    "basis"_a=HFABasis::PARTICLE_HOLE,
                    "variant"_a= HFAVariant::ONE);
        }
    }

    void bindActions(py::module &mod) {
        py::module actmod = mod.def_submodule("action", "Actions");

        auto action = bindBaseAction(actmod);
        bindSumAction(actmod, action);
        bindHubbardGaugeAction(actmod, action);
        bindHubbardFermiAction(actmod, action);
    }
}
