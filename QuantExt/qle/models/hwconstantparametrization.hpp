/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file hwconstantparametrization.hpp
    \brief Hull White n factor parametrization with constant reversion and vol
    \ingroup models
*/

#pragma once

#include <qle/models/hwparametrization.hpp>

namespace QuantExt {

//! HW nF Parametrization with m driving Brownian motions and constant reversion, vol
/*! \ingroup models
 */
template <class TS> class HwConstantParametrization : public HwParametrization<TS> {
public:
    HwConstantParametrization(QuantLib::Array kappa, QuantLib::Matrix sigma, const QuantLib::Currency& currency,
                              const QuantLib::Handle<TS>& termStructure,
                              const std::string& name = std::string());

    QuantLib::Array kappa(const QuantLib::Time t) const override { return kappa_; };
    QuantLib::Matrix sigma_x(const QuantLib::Time t) const override { return sigma_; }
    QuantLib::Matrix y(const QuantLib::Time t) const override;
    QuantLib::Array g(const QuantLib::Time t, const QuantLib::Time T) const override;

private:
    static constexpr QuantLib::Real zeroKappaCutoff_ = 1.0E-6;
    QuantLib::Array kappa_;
    QuantLib::Matrix sigma_;
};

// implementation

template <class TS>
HwConstantParametrization<TS>::HwConstantParametrization(QuantLib::Array kappa, QuantLib::Matrix sigma,
                                                         const QuantLib::Currency& currency,
                                                         const QuantLib::Handle<TS>& termStructure,
                                                         const std::string& name)
    : HwParametrization<TS>(kappa.size(), sigma.rows(), currency, termStructure, name.empty() ? currency.code() : name),
      kappa_(std::move(kappa)), sigma_(std::move(sigma)) {
    QL_REQUIRE(sigma.columns() == kappa.size(), "HwConstantParametrization: sigma ("
                                                    << sigma.rows() << "x" << sigma.columns()
                                                    << ") not consistent with kappa (" << kappa.size() << ")");
}

template <class TS> QuantLib::Matrix HwConstantParametrization<TS>::y(const QuantLib::Time t) const {
    QuantLib::Matrix y(this->n_, this->n_, 0.0);
    for (Size i = 0; i < this->n_; ++i) {
        for (Size j = 0; j <= i; ++j) {
            QuantLib::Real tmp;
            if (std::abs(kappa_[i] + kappa_[j]) < zeroKappaCutoff_) {
                tmp = t;
            } else {
                tmp = (1.0 - std::exp(-(kappa_[i] + kappa_[j]) * t)) / (kappa_[i] + kappa_[j]);
            }
            for (Size k = 0; k < this->m_; ++k) {
                y(i, j) += sigma_x(t)(i, k) * sigma_x(t)(j, k) * tmp;
            }
            if (j < i)
                y(j, i) = y(i, j);
        }
    }
    return y;
}

template <class TS>
QuantLib::Array HwConstantParametrization<TS>::g(const QuantLib::Time t, const QuantLib::Time T) const {
    QL_REQUIRE(t <= T, "HwConstantParametrization::g(" << t << "," << T << ") invalid, expected t < T");
    QuantLib::Array g(this->n_, 0.0);
    for (Size i = 0; i < this->n_; ++i) {
        if (std::abs(kappa_[i]) < zeroKappaCutoff_) {
            g[i] = (1.0 - std::exp(-kappa_[i] * (T - t)));
        } else {
            g[i] = T - t;
        }
    }
    return g;
}

// typedef

typedef HwConstantParametrization<YieldTermStructure> IrHwConstantParametrization;

} // namespace QuantExt
