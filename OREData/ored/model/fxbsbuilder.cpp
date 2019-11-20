/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/quotes/simplequote.hpp>

#include <qle/models/fxbsconstantparametrization.hpp>
#include <qle/models/fxbspiecewiseconstantparametrization.hpp>
#include <qle/models/fxeqoptionhelper.hpp>
#include <qle/pricingengines/analyticcclgmfxoptionengine.hpp>

#include <ored/model/fxbsbuilder.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/strike.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
namespace data {

FxBsBuilder::FxBsBuilder(const boost::shared_ptr<ore::data::Market>& market, const boost::shared_ptr<FxBsData>& data,
                         const std::string& configuration)
    : market_(market), configuration_(configuration), data_(data) {

    marketObserver_ = boost::make_shared<MarketObserver>();
    QuantLib::Currency ccy = ore::data::parseCurrency(data->foreignCcy());
    QuantLib::Currency domesticCcy = ore::data::parseCurrency(data->domesticCcy());
    std::string ccyPair = ccy.code() + domesticCcy.code();

    // get market data
    fxSpot_ = market_->fxSpot(ccyPair, configuration_);
    ytsDom_ = market_->discountCurve(domesticCcy.code(), configuration_);
    ytsFor_ = market_->discountCurve(ccy.code(), configuration_);
    fxVol_ = market_->fxVol(ccyPair, configuration_);

    // register with market observables except vols
    marketObserver_->addObservable(fxSpot_);
    marketObserver_->addObservable(market_->discountCurve(domesticCcy.code()));
    marketObserver_->addObservable(market_->discountCurve(ccy.code()));

    // register the builder with the market observer
    registerWith(marketObserver_);

    // build option basket and derive parametrization from it
    if (data->calibrateSigma())
        buildOptionBasket();

    Array sigmaTimes, sigma;
    if (data->sigmaParamType() == ParamType::Constant) {
        QL_REQUIRE(data->sigmaTimes().size() == 0, "empty sigma tme grid expected");
        QL_REQUIRE(data->sigmaValues().size() == 1, "initial sigma grid size 1 expected");
        sigmaTimes = Array(0);
        sigma = Array(data_->sigmaValues().begin(), data_->sigmaValues().end());
    } else {
        if (data->calibrateSigma() && data->calibrationType() == CalibrationType::Bootstrap) { // override
            QL_REQUIRE(optionExpiries_.size() > 0, "optionExpiries is empty");
            sigmaTimes = Array(optionExpiries_.begin(), optionExpiries_.end() - 1);
            sigma = Array(sigmaTimes.size() + 1, data->sigmaValues()[0]);
        } else {
            // use input time grid and input alpha array otherwise
            sigmaTimes = Array(data_->sigmaTimes().begin(), data_->sigmaTimes().end());
            sigma = Array(data_->sigmaValues().begin(), data_->sigmaValues().end());
            QL_REQUIRE(sigma.size() == sigmaTimes.size() + 1, "sigma grids do not match");
        }
    }

    if (data->sigmaParamType() == ParamType::Piecewise)
        parametrization_ =
            boost::make_shared<QuantExt::FxBsPiecewiseConstantParametrization>(ccy, fxSpot_, sigmaTimes, sigma);
    else if (data->sigmaParamType() == ParamType::Constant)
        parametrization_ = boost::make_shared<QuantExt::FxBsConstantParametrization>(ccy, fxSpot_, sigma[0]);
    else
        QL_FAIL("interpolation type not supported for FX");

}

Real FxBsBuilder::error() const {
    calculate();
    return error_;
}

boost::shared_ptr<QuantExt::FxBsParametrization> FxBsBuilder::parametrization() const {
    calculate();
    return parametrization_;
}
std::vector<boost::shared_ptr<BlackCalibrationHelper>> FxBsBuilder::optionBasket() const {
    calculate();
    return optionBasket_;
}

bool FxBsBuilder::requiresRecalibration() const {
    return (data_->calibrateSigma() && volSurfaceChanged(false)) || marketObserver_->hasUpdated(false) ||
           forceCalibration_;
}

void FxBsBuilder::performCalculations() const {
    if (requiresRecalibration()) {
        // update vol cache
        volSurfaceChanged(true);
        // reset market observer updated flag
        marketObserver_->hasUpdated(true);
    }
}

Real FxBsBuilder::optionStrike(const Size j) const {
    ore::data::Strike strike = ore::data::parseStrike(data_->optionStrikes()[j]);
    Real strikeValue;
    // TODO: Extend strike type coverage
    if (strike.type == ore::data::Strike::Type::ATMF)
        strikeValue = Null<Real>();
    else if (strike.type == ore::data::Strike::Type::Absolute)
        strikeValue = strike.value;
    else
        QL_FAIL("strike type ATMF or Absolute expected");
    return strikeValue;
}

Date FxBsBuilder::optionExpiry(const Size j) const {
    Date today = Settings::instance().evaluationDate();
    std::string expiryString = data_->optionExpiries()[j];
    bool expiryDateBased;
    Period expiryPb;
    Date expiryDb;
    parseDateOrPeriod(expiryString, expiryDb, expiryPb, expiryDateBased);
    Date expiryDate = expiryDateBased ? expiryDb : today + expiryPb;
    return expiryDate;
}

bool FxBsBuilder::volSurfaceChanged(const bool updateCache) const {
    bool hasUpdated = false;

    // if cache doesn't exist resize vector
    if (fxVolCache_.size() != data_->optionExpiries().size())
        fxVolCache_ = vector<Real>(data_->optionExpiries().size());

    std::vector<Time> expiryTimes(data_->optionExpiries().size());
    for (Size j = 0; j < data_->optionExpiries().size(); j++) {
        Real vol = fxVol_->blackVol(optionExpiry(j), optionStrike(j));
        if (!close_enough(fxVolCache_[j], vol)) {
            if (updateCache)
                fxVolCache_[j] = vol;
            hasUpdated = true;
        }
    }
    return hasUpdated;
}

void FxBsBuilder::buildOptionBasket() const {
    QL_REQUIRE(data_->optionExpiries().size() == data_->optionStrikes().size(), "fx option vector size mismatch");
    std::vector<Time> expiryTimes(data_->optionExpiries().size());
    for (Size j = 0; j < data_->optionExpiries().size(); j++) {
        Date expiryDate = optionExpiry(j);
        Real strikeValue = optionStrike(j);
        Handle<Quote> quote(boost::make_shared<SimpleQuote>(fxVol_->blackVol(expiryDate, strikeValue)));
        boost::shared_ptr<QuantExt::FxEqOptionHelper> helper =
            boost::make_shared<QuantExt::FxEqOptionHelper>(expiryDate, strikeValue, fxSpot_, quote, ytsDom_, ytsFor_);
        optionBasket_.push_back(helper);
        helper->performCalculations();
        expiryTimes[j] = ytsDom_->timeFromReference(helper->option()->exercise()->date(0));
        LOG("Added FxEqOptionHelper " << (data_->foreignCcy() + data_->domesticCcy()) << " "
                                      << QuantLib::io::iso_date(expiryDate) << " " << helper->strike() << " "
                                      << quote->value());
    }

    std::sort(expiryTimes.begin(), expiryTimes.end());
    auto itExpiryTime = unique(expiryTimes.begin(), expiryTimes.end());
    expiryTimes.resize(distance(expiryTimes.begin(), itExpiryTime));

    optionExpiries_ = Array(expiryTimes.size());
    for (Size j = 0; j < expiryTimes.size(); j++)
        optionExpiries_[j] = expiryTimes[j];
}

void FxBsBuilder::forceRecalculate() {
    forceCalibration_ = true;
    ModelBuilder::forceRecalculate();
    forceCalibration_ = false;
}
} // namespace data
} // namespace ore
