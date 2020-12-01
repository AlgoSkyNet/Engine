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

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/equityoption.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/equityoption.hpp>
#include <ored/portfolio/referencedata.hpp>
#include <ored/utilities/currencycheck.hpp>
#include <ored/utilities/log.hpp>
#include <ql/errors.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/compositeinstrument.hpp>
#include <ql/instruments/vanillaoption.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void EquityOption::build(const boost::shared_ptr<EngineFactory>& engineFactory) {

    // Set the assetName_ as it may have changed after lookup
    assetName_ = equityName();

    // Populate the index_ in case the option is automatic exercise.
    const boost::shared_ptr<Market>& market = engineFactory->market();
    index_ = *market->equityCurve(assetName_, engineFactory->configuration(MarketContext::pricing));
    
    // check the equity currency
    Currency equityCurrency = market->equityCurve(assetName_, engineFactory->configuration(MarketContext::pricing))->currency();
    QL_REQUIRE(!equityCurrency.empty(), "No equity currency in equityCurve for equity " << assetName_);
    QL_REQUIRE(equityCurrency == parseCurrencyWithMinors(currency_), 
        "EquityCurrency " << equityCurrency << " must match Option currency " << currency_ << " for trade " << id());

    // Build the trade using the shared functionality in the base class.
    VanillaOptionTrade::build(engineFactory);

    // LOG the volatility if the trade expiry date is in the future.
    if (expiryDate_ > Settings::instance().evaluationDate()) {
        DLOG("Implied vol for " << tradeType_ << " on " << assetName_ << " with expiry " << expiryDate_
                                << " and strike " << strike_ << " is "
                                << market->equityVol(assetName_)->blackVol(expiryDate_, strike_));
    }
}

void EquityOption::setCcyStrike() {
    Currency ccy = parseCurrencyWithMinors(localCurrency_);
    currency_ = ccy.code();
    // if we have a minor currency, convert the strike
    if (!strikeCurrency_.empty()) {
        QL_REQUIRE(parseCurrencyWithMinors(strikeCurrency_) == ccy,
            "Stike currency " << strikeCurrency_ << " does not match option currency " << currency_ << " for trade " << id());
        strike_ = convertMinorToMajorCurrency(strikeCurrency_, strike_);
    } else {
        DLOG("No StrikeCurrency provided, using Option currency " << localCurrency_);
        strike_ = convertMinorToMajorCurrency(localCurrency_, localStrike_);
    }
}

void EquityOption::fromXML(XMLNode* node) {
    VanillaOptionTrade::fromXML(node);
    XMLNode* eqNode = XMLUtils::getChildNode(node, "EquityOptionData");
    QL_REQUIRE(eqNode, "No EquityOptionData Node");
    option_.fromXML(XMLUtils::getChildNode(eqNode, "OptionData"));
    XMLNode* tmp = XMLUtils::getChildNode(eqNode, "Underlying");
    if (!tmp)
        tmp = XMLUtils::getChildNode(eqNode, "Name");
    equityUnderlying_.fromXML(tmp);
    localCurrency_ = XMLUtils::getChildValue(eqNode, "Currency", true);
    localStrike_ = XMLUtils::getChildValueAsDouble(eqNode, "Strike", true);
    strikeCurrency_ = XMLUtils::getChildValue(eqNode, "StrikeCurrency", false);
    quantity_ = XMLUtils::getChildValueAsDouble(eqNode, "Quantity", true);
    setCcyStrike();
}

XMLNode* EquityOption::toXML(XMLDocument& doc) {
    XMLNode* node = VanillaOptionTrade::toXML(doc);
    XMLNode* eqNode = doc.allocNode("EquityOptionData");
    XMLUtils::appendNode(node, eqNode);

    XMLUtils::appendNode(eqNode, option_.toXML(doc));
    XMLUtils::appendNode(eqNode, equityUnderlying_.toXML(doc));
    XMLUtils::addChild(doc, eqNode, "Currency", localCurrency_);
    XMLUtils::addChild(doc, eqNode, "Strike", localStrike_);
    if (!strikeCurrency_.empty())
        XMLUtils::addChild(doc, eqNode, "StrikeCurrency", strikeCurrency_);
    XMLUtils::addChild(doc, eqNode, "Quantity", quantity_);

    return node;
}

std::map<AssetClass, std::set<std::string>> EquityOption::underlyingIndices() const {
    return {{AssetClass::EQ, std::set<std::string>({equityName()})}};
}

} // namespace data
} // namespace ore
