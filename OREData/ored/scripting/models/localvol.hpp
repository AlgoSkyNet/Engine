/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of ORE, a free-software/open-source library
 for transparent pricing and risk analysis - http://opensourcerisk.org

 ORE is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program.
 The license is also available online at <http://opensourcerisk.org>

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

/*! \file ored/scripting/models/localvol.hpp
    \brief local vol model for n underlyings (fx, equity or commodity)
    \ingroup utilities
*/

#pragma once

#include <ored/scripting/models/blackscholesbase.hpp>

namespace ore {
namespace data {

class LocalVol : public BlackScholesBase {
public:
    /* ctor for multiple underlyings, see BlackScholesBase, plus:
       - processes: hold spot, rate and div ts and vol for each given index
       - calibrationMoneyness: a vector of relative forward atm moneyness used to calibrate the Andrease-Huge volatility
         surface to
       - we assume that the given correlations are constant and read the value only at t = 0
    */
    LocalVol(
        const Size paths, const std::vector<std::string>& currencies,
        const std::vector<Handle<YieldTermStructure>>& curves, const std::vector<Handle<Quote>>& fxSpots,
        const std::vector<std::pair<std::string, boost::shared_ptr<InterestRateIndex>>>& irIndices,
        const std::vector<std::pair<std::string, boost::shared_ptr<ZeroInflationIndex>>>& infIndices,
        const std::vector<std::string>& indices, const std::vector<std::string>& indexCurrencies,
        const Handle<BlackScholesModelWrapper>& model,
        const std::map<std::pair<std::string, std::string>, Handle<QuantExt::CorrelationTermStructure>>& correlations,
        const Size regressionOrder, const std::set<Date>& simulationDates,
        const IborFallbackConfig& iborFallbackConfig = IborFallbackConfig::defaultConfig());

    // ctor for a single underlying
    LocalVol(const Size paths, const std::string& currency, const Handle<YieldTermStructure>& curve,
             const std::string& index, const std::string& indexCurrency, const Handle<BlackScholesModelWrapper>& model,
             const Size regressionOrder, const std::set<Date>& simulationDates,
             const IborFallbackConfig& iborFallbackConfig = IborFallbackConfig::defaultConfig());

private:
    // ModelImpl interface implementation
    RandomVariable getFutureBarrierProb(const std::string& index, const Date& obsdate1, const Date& obsdate2,
                                        const RandomVariable& barrier, const bool above) const override {
        QL_FAIL("getFutureBarrierProb not implemented by LocalVol");
    }
    // BlackScholesBase interface implementation
    void performCalculations() const override;
};

} // namespace data
} // namespace ore
