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

#include <algorithm>
#include <boost/algorithm/string/join.hpp>
#include <boost/range/adaptor/indexed.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <ored/marketdata/equityvolcurve.hpp>
#include <ored/marketdata/marketdatumparser.hpp>
#include <ored/utilities/currencycheck.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/utilities/wildcard.hpp>
#include <ql/math/interpolations/loginterpolation.hpp>
#include <ql/math/matrix.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancecurve.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancesurface.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <qle/models/carrmadanarbitragecheck.hpp>
#include <qle/termstructures/blackdeltautilities.hpp>
#include <qle/termstructures/blackvariancesurfacemoneyness.hpp>
#include <qle/termstructures/blackvariancesurfacesparse.hpp>
#include <qle/termstructures/blackvolsurfacedelta.hpp>
#include <qle/termstructures/eqcommoptionsurfacestripper.hpp>
#include <qle/termstructures/equityblackvolsurfaceproxy.hpp>
#include <qle/termstructures/optionpricesurface.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
namespace data {

EquityVolCurve::EquityVolCurve(Date asof, EquityVolatilityCurveSpec spec, const Loader& loader,
                               const CurveConfigurations& curveConfigs, const Handle<EquityIndex>& eqIndex,
                               const map<string, boost::shared_ptr<EquityCurve>>& requiredEquityCurves,
                               const map<string, boost::shared_ptr<EquityVolCurve>>& requiredEquityVolCurves) {

    try {
        LOG("EquityVolCurve: start building equity volatility structure with ID " << spec.curveConfigID());

        auto config = *curveConfigs.equityVolCurveConfig(spec.curveConfigID());

        calendar_ = parseCalendar(config.calendar());
        // if calendar is null use currency
        if (calendar_ == NullCalendar())
            calendar_ = parseCalendar(config.ccy());
        dayCounter_ = parseDayCounter(config.dayCounter());

        if (config.isProxySurface()) {
            buildVolatility(asof, spec, curveConfigs, requiredEquityCurves, requiredEquityVolCurves);
        } else {
            QL_REQUIRE(config.quoteType() == MarketDatum::QuoteType::PRICE ||
                           config.quoteType() == MarketDatum::QuoteType::RATE_LNVOL,
                       "EquityVolCurve: Only lognormal volatilities and option premiums supported for equity "
                       "volatility surfaces.");

            // Do different things depending on the type of volatility configured
            boost::shared_ptr<VolatilityConfig> vc = config.volatilityConfig();
            if (auto cvc = boost::dynamic_pointer_cast<ConstantVolatilityConfig>(vc)) {
                buildVolatility(asof, config, *cvc, loader);
            } else if (auto vcc = boost::dynamic_pointer_cast<VolatilityCurveConfig>(vc)) {
                buildVolatility(asof, config, *vcc, loader);
            } else if (auto vssc = boost::dynamic_pointer_cast<VolatilityStrikeSurfaceConfig>(vc)) {
                buildVolatility(asof, config, *vssc, loader, eqIndex);
            } else if (auto vmsc = boost::dynamic_pointer_cast<VolatilityMoneynessSurfaceConfig>(vc)) {
                buildVolatility(asof, config, *vmsc, loader, eqIndex);
            } else if (auto vdsc = boost::dynamic_pointer_cast<VolatilityDeltaSurfaceConfig>(vc)) {
                buildVolatility(asof, config, *vdsc, loader, eqIndex);
            } else {
                QL_FAIL("Unexpected VolatilityConfig in EquityVolatilityConfig");
            }
        }
        DLOG("EquityVolCurve: finished building equity volatility structure with ID " << spec.curveConfigID());

        buildCalibrationInfo(asof, curveConfigs, config, eqIndex);

    } catch (exception& e) {
        QL_FAIL("Equity volatility curve building failed : " << e.what());
    } catch (...) {
        QL_FAIL("Equity volatility curve building failed: unknown error");
    }
}

void EquityVolCurve::buildVolatility(const Date& asof, const EquityVolatilityCurveConfig& vc,
                                     const ConstantVolatilityConfig& cvc, const Loader& loader) {
    LOG("EquityVolCurve: start building constant volatility structure");

    QL_REQUIRE(cvc.quoteType() == MarketDatum::QuoteType::RATE_LNVOL ||
                   cvc.quoteType() == MarketDatum::QuoteType::RATE_SLNVOL ||
                   cvc.quoteType() == MarketDatum::QuoteType::RATE_NVOL,
               "Quote for Equity Constant Volatility Config must be a Volatility");

    // Loop over all market datums and find the single quote
    // Return error if there are duplicates (this is why we do not use loader.get() method)
    Real quoteValue = Null<Real>();
    for (const boost::shared_ptr<MarketDatum>& md : loader.loadQuotes(asof)) {
        if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::EQUITY_OPTION) {

            boost::shared_ptr<EquityOptionQuote> q = boost::dynamic_pointer_cast<EquityOptionQuote>(md);

            if (q->name() == cvc.quote()) {
                TLOG("Found the constant volatility quote " << q->name());
                QL_REQUIRE(quoteValue == Null<Real>(), "Duplicate quote found for quote with id " << cvc.quote());
                // convert quote from minor to major currency if needed
                quoteValue = convertMinorToMajorCurrency(q->ccy(), q->quote()->value());
            }
        }
    }
    QL_REQUIRE(quoteValue != Null<Real>(), "Quote not found for id " << cvc.quote());

    DLOG("Creating BlackConstantVol structure");
    vol_ = boost::make_shared<BlackConstantVol>(asof, calendar_, quoteValue, dayCounter_);

    LOG("EquityVolCurve: finished building constant volatility structure");
}

void EquityVolCurve::buildVolatility(const Date& asof, const EquityVolatilityCurveConfig& vc,
                                     const VolatilityCurveConfig& vcc, const Loader& loader) {

    LOG("EquityVolCurve: start building 1-D volatility curve");

    QL_REQUIRE(vcc.quoteType() == MarketDatum::QuoteType::RATE_LNVOL ||
                   vcc.quoteType() == MarketDatum::QuoteType::RATE_SLNVOL ||
                   vcc.quoteType() == MarketDatum::QuoteType::RATE_NVOL,
               "Quote for Equity Constant Volatility Config must be a Volatility");

    // Must have at least one quote
    QL_REQUIRE(vcc.quotes().size() > 0, "No quotes specified in config " << vc.curveID());

    // Check if we are using a regular expression to select the quotes for the curve. If we are, the quotes should
    // contain exactly one element.
    auto wildcard = getUniqueWildcard(vcc.quotes());

    // curveData will be populated with the expiry dates and volatility values.
    map<Date, Real> curveData;

    // Different approaches depending on whether we are using a regex or searching for a list of explicit quotes.
    if (wildcard) {
        DLOG("Have single quote with pattern " << (*wildcard).regex());

        // Loop over quotes and process commodity option quotes matching pattern on asof
        for (const boost::shared_ptr<MarketDatum>& md : loader.loadQuotes(asof)) {

            // Go to next quote if the market data point's date does not equal our asof
            if (md->asofDate() != asof)
                continue;

            auto q = boost::dynamic_pointer_cast<EquityOptionQuote>(md);
            if (q && (*wildcard).matches(q->name()) && q->quoteType() == vc.quoteType()) {

                TLOG("The quote " << q->name() << " matched the pattern");

                Date expiryDate = getDateFromDateOrPeriod(q->expiry(), asof, calendar_);
                if (expiryDate > asof) {
                    // Add the quote to the curve data
                    QL_REQUIRE(curveData.count(expiryDate) == 0, "Duplicate quote for the expiry date "
                                                                     << io::iso_date(expiryDate)
                                                                     << " provided by equity volatility config "
                                                                     << vc.curveID());
                    // convert quote from minor to major currency if needed
                    curveData[expiryDate] = convertMinorToMajorCurrency(q->ccy(), q->quote()->value());

                    TLOG("Added quote " << q->name() << ": (" << io::iso_date(expiryDate) << "," << fixed
                                        << setprecision(9) << q->quote()->value() << ")");
                }
            }
        }
        // Check that we have quotes in the end
        QL_REQUIRE(curveData.size() > 0, "No quotes found matching regular expression " << vcc.quotes()[0]);

    } else {

        DLOG("Have " << vcc.quotes().size() << " explicit quotes");

        // Loop over quotes and process commodity option quotes that are explicitly specified in the config
        for (const boost::shared_ptr<MarketDatum>& md : loader.loadQuotes(asof)) {
            // Go to next quote if the market data point's date does not equal our asof
            if (md->asofDate() != asof)
                continue;

            if (auto q = boost::dynamic_pointer_cast<EquityOptionQuote>(md)) {

                // Find quote name in configured quotes.
                auto it = find(vcc.quotes().begin(), vcc.quotes().end(), q->name());

                if (it != vcc.quotes().end()) {
                    TLOG("Found the configured quote " << q->name());

                    Date expiryDate = getDateFromDateOrPeriod(q->expiry(), asof, calendar_);
                    QL_REQUIRE(expiryDate > asof, "Equity volatility quote '" << q->name()
                                                                              << "' has expiry in the past ("
                                                                              << io::iso_date(expiryDate) << ")");
                    QL_REQUIRE(curveData.count(expiryDate) == 0, "Duplicate quote for the date "
                                                                     << io::iso_date(expiryDate)
                                                                     << " provided by equity volatility config "
                                                                     << vc.curveID());

                    // convert quote from minor to major currency if needed
                    curveData[expiryDate] = convertMinorToMajorCurrency(q->ccy(), q->quote()->value());

                    TLOG("Added quote " << q->name() << ": (" << io::iso_date(expiryDate) << "," << fixed
                                        << setprecision(9) << q->quote()->value() << ")");
                }
            }
        }

        // Check that we have found all of the explicitly configured quotes
        QL_REQUIRE(curveData.size() == vcc.quotes().size(), "Found " << curveData.size() << " quotes, but "
                                                                     << vcc.quotes().size()
                                                                     << " quotes were given in config.");
    }

    // Create the dates and volatility vector
    vector<Date> dates;
    vector<Volatility> volatilities;
    for (const auto& datum : curveData) {
        dates.push_back(datum.first);
        volatilities.push_back(datum.second);
        TLOG("Added data point (" << io::iso_date(dates.back()) << "," << fixed << setprecision(9)
                                  << volatilities.back() << ")");
    }

    DLOG("Creating BlackVarianceCurve object.");
    auto tmp = boost::make_shared<BlackVarianceCurve>(asof, dates, volatilities, dayCounter_);

    // Set the interpolation.
    if (vcc.interpolation() == "Linear") {
        DLOG("Interpolation set to Linear.");
    } else if (vcc.interpolation() == "Cubic") {
        DLOG("Setting interpolation to Cubic.");
        tmp->setInterpolation<Cubic>();
    } else if (vcc.interpolation() == "LogLinear") {
        DLOG("Setting interpolation to LogLinear.");
        tmp->setInterpolation<LogLinear>();
    } else {
        DLOG("Interpolation " << vcc.interpolation() << " not recognised so leaving it Linear.");
    }

    // Set the volatility_ member after we have possibly updated the interpolation.
    vol_ = tmp;

    // Set the extrapolation
    if (parseExtrapolation(vcc.extrapolation()) == Extrapolation::Flat) {
        DLOG("Enabling BlackVarianceCurve flat volatility extrapolation.");
        vol_->enableExtrapolation();
    } else if (parseExtrapolation(vcc.extrapolation()) == Extrapolation::None) {
        DLOG("Disabling BlackVarianceCurve extrapolation.");
        vol_->disableExtrapolation();
    } else if (parseExtrapolation(vcc.extrapolation()) == Extrapolation::UseInterpolator) {
        DLOG("BlackVarianceCurve does not support using interpolator for extrapolation "
             << "so default to flat volatility extrapolation.");
        vol_->enableExtrapolation();
    } else {
        DLOG("Unexpected extrapolation so default to flat volatility extrapolation.");
        vol_->enableExtrapolation();
    }

    LOG("EquityVolCurve: finished building 1-D volatility curve");
}

void EquityVolCurve::buildVolatility(const Date& asof, EquityVolatilityCurveConfig& vc,
                                     const VolatilityStrikeSurfaceConfig& vssc, const Loader& loader,
                                     const QuantLib::Handle<EquityIndex>& eqIndex) {
    try {

        QL_REQUIRE(vssc.expiries().size() > 0, "No expiries defined");
        QL_REQUIRE(vssc.strikes().size() > 0, "No strikes defined");

        // check for wild cards
        bool expiriesWc = find(vssc.expiries().begin(), vssc.expiries().end(), "*") != vssc.expiries().end();
        bool strikesWc = find(vssc.strikes().begin(), vssc.strikes().end(), "*") != vssc.strikes().end();
        if (expiriesWc) {
            QL_REQUIRE(vssc.expiries().size() == 1, "Wild card expiriy specified but more expiries also specified.");
        }
        if (strikesWc) {
            QL_REQUIRE(vssc.strikes().size() == 1, "Wild card strike specified but more strikes also specified.");
        }
        bool wildcard = strikesWc || expiriesWc;

        vector<Real> callStrikes, putStrikes;
        vector<Real> callData, putData;
        vector<Date> callExpiries, putExpiries;

        // In case of wild card we need the following granularity within the mkt data loop
        bool strikeRelevant = strikesWc;
        bool expiryRelevant = expiriesWc;
        bool quoteRelevant = true;

        // We loop over all market data, looking for quotes that match the configuration
        Size callQuotesAdded = 0;
        Size putQuotesAdded = 0;
        for (auto& md : loader.loadQuotes(asof)) {
            // skip irrelevant data
            if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::EQUITY_OPTION &&
                md->quoteType() == vc.quoteType()) {
                boost::shared_ptr<EquityOptionQuote> q = boost::dynamic_pointer_cast<EquityOptionQuote>(md);
                // todo - for now we will ignore ATM, ATMF quotes both for explicit strikes and in case of strike wild
                // card. ----
                auto absoluteStrike = boost::dynamic_pointer_cast<AbsoluteStrike>(q->strike());
                if (absoluteStrike && (q->eqName() == vc.curveID() && q->ccy() == vc.ccy())) {
                    if (!expiriesWc) {
                        auto j = std::find(vssc.expiries().begin(), vssc.expiries().end(), q->expiry());
                        expiryRelevant = j != vssc.expiries().end();
                    }
                    if (!strikesWc) {
                        auto i = std::find_if(vssc.strikes().begin(), vssc.strikes().end(),
                                              [&absoluteStrike](const std::string& x) {
                                                  return close_enough(parseReal(x), absoluteStrike->strike());
                                              });
                        strikeRelevant = i != vssc.strikes().end();
                    }
                    quoteRelevant = strikeRelevant && expiryRelevant;

                    // add quote to vectors, if relevant
                    // If a quote doesn't include a call/put flag (an Implied Vol for example), it
                    // defaults to a call. For an explicit surface we expect either a call and put
                    // for every point, or just a vol at every point
                    if (quoteRelevant) {
                        Date tmpDate = getDateFromDateOrPeriod(q->expiry(), asof, calendar_);
                        QL_REQUIRE(tmpDate >= asof,
                                   "Option quote for a past date (" << ore::data::to_string(tmpDate) << ")");
                        if (tmpDate == asof) {
                            DLOG("Option quote for as of date (" << ore::data::to_string(tmpDate) << ") ignored.");
                            continue;
                        }
                        // get values and strikes, convert from minor to major currency if needed
                        Real quoteValue = q->quote()->value();
                        if (vc.quoteType() == MarketDatum::QuoteType::PRICE)
                            quoteValue = convertMinorToMajorCurrency(q->ccy(), quoteValue);
                        Real strikeValue = convertMinorToMajorCurrency(q->ccy(), absoluteStrike->strike());

                        if (q->isCall()) {
                            callStrikes.push_back(strikeValue);
                            callData.push_back(quoteValue);
                            callExpiries.push_back(tmpDate);
                            callQuotesAdded++;
                        } else {
                            putStrikes.push_back(strikeValue);
                            putData.push_back(quoteValue);
                            putExpiries.push_back(tmpDate);
                            putQuotesAdded++;
                        }
                    }
                }
            }
        }

        QL_REQUIRE(callQuotesAdded > 0, "No valid equity volatility quotes provided");
        bool callSurfaceOnly = false;
        if (callQuotesAdded > 0 && putQuotesAdded == 0) {
            QL_REQUIRE(vc.quoteType() != MarketDatum::QuoteType::PRICE,
                       "For Premium quotes, call and put quotes must be supplied.");
            DLOG("EquityVolatilityCurve " << vc.curveID() << ": Only one set of quotes, can build surface directly");
            callSurfaceOnly = true;
        }
        // Check loaded quotes
        if (!wildcard) {
            Size explicitGridSize = vssc.expiries().size() * vssc.strikes().size();
            QL_REQUIRE(callQuotesAdded == explicitGridSize,
                       "EquityVolatilityCurve " << vc.curveID() << ": " << callQuotesAdded << " quotes provided but "
                                                << explicitGridSize << " expected.");
            if (!callSurfaceOnly) {
                QL_REQUIRE(callQuotesAdded == putQuotesAdded,
                           "Call and Put quotes must match for explicitly defined surface, "
                               << callQuotesAdded << " call quotes, and " << putQuotesAdded << " put quotes");
                DLOG("EquityVolatilityCurve " << vc.curveID() << ": Complete set of " << callQuotesAdded
                                              << ", call and put quotes found.");
            }
        }

        QL_REQUIRE(callStrikes.size() == callData.size() && callData.size() == callExpiries.size(),
                   "Quotes loaded don't produce strike,vol,expiry vectors of equal length.");
        QL_REQUIRE(putStrikes.size() == putData.size() && putData.size() == putExpiries.size(),
                   "Quotes loaded don't produce strike,vol,expiry vectors of equal length.");
        DLOG("EquityVolatilityCurve " << vc.curveID() << ": Found " << callQuotesAdded << ", call quotes and "
                                      << putQuotesAdded << " put quotes using wildcard.");

        // Set the strike extrapolation which only matters if extrapolation is turned on for the whole surface.
        bool flatStrikeExtrap = true;
        bool flatTimeExtrap = true;
        if (vssc.extrapolation()) {

            auto strikeExtrapType = parseExtrapolation(vssc.strikeExtrapolation());
            if (strikeExtrapType == Extrapolation::UseInterpolator) {
                DLOG("Strike extrapolation switched to using interpolator.");
                flatStrikeExtrap = false;
            } else if (strikeExtrapType == Extrapolation::None) {
                DLOG("Strike extrapolation cannot be turned off on its own so defaulting to flat.");
            } else if (strikeExtrapType == Extrapolation::Flat) {
                DLOG("Strike extrapolation has been set to flat.");
            } else {
                DLOG("Strike extrapolation " << strikeExtrapType << " not expected so default to flat.");
            }

            auto timeExtrapType = parseExtrapolation(vssc.timeExtrapolation());
            if (timeExtrapType == Extrapolation::UseInterpolator) {
                DLOG("Time extrapolation switched to using interpolator.");
                flatTimeExtrap = false;
            } else if (timeExtrapType == Extrapolation::None) {
                DLOG("Time extrapolation cannot be turned off on its own so defaulting to flat.");
            } else if (timeExtrapType == Extrapolation::Flat) {
                DLOG("Time extrapolation has been set to flat.");
            } else {
                DLOG("Time extrapolation " << timeExtrapType << " not expected so default to flat.");
            }

        } else {
            DLOG("Extrapolation is turned off for the whole surface so the time and"
                 << " strike extrapolation settings are ignored");
        }

        bool preferOutOfTheMoney = vc.preferOutOfTheMoney() && *vc.preferOutOfTheMoney();

        if (vc.quoteType() == MarketDatum::QuoteType::PRICE) {

            // Create the 1D solver options used in the price stripping.
            Solver1DOptions solverOptions = vc.solverConfig();

            DLOG("Building a option price surface for calls and puts");
            boost::shared_ptr<OptionPriceSurface> callSurface =
                boost::make_shared<OptionPriceSurface>(asof, callExpiries, callStrikes, callData, dayCounter_);
            boost::shared_ptr<OptionPriceSurface> putSurface =
                boost::make_shared<OptionPriceSurface>(asof, putExpiries, putStrikes, putData, dayCounter_);

            DLOG("CallSurface contains " << callSurface->expiries().size() << " expiries.");

            DLOG("Stripping equity volatility surface from the option premium surfaces");
            boost::shared_ptr<EquityOptionSurfaceStripper> eoss = boost::make_shared<EquityOptionSurfaceStripper>(
                eqIndex, callSurface, putSurface, calendar_, dayCounter_, vc.exerciseType(), flatStrikeExtrap,
                flatStrikeExtrap, flatTimeExtrap, preferOutOfTheMoney, solverOptions);
            vol_ = eoss->volSurface();

        } else if (vc.quoteType() == MarketDatum::QuoteType::RATE_LNVOL) {

            if (callExpiries.size() == 1 && callStrikes.size() == 1) {
                LOG("EquityVolCurve: Building BlackConstantVol");
                vol_ = boost::shared_ptr<BlackVolTermStructure>(
                    new BlackConstantVol(asof, Calendar(), callData[0], dayCounter_));
            } else {
                // create a vol surface from the calls
                boost::shared_ptr<BlackVarianceSurfaceSparse> callSurface =
                    boost::make_shared<BlackVarianceSurfaceSparse>(asof, calendar_, callExpiries, callStrikes, callData,
                                                                   dayCounter_, flatStrikeExtrap, flatStrikeExtrap,
                                                                   flatTimeExtrap);

                if (callSurfaceOnly) {
                    // if only a call surface provided use that
                    vol_ = callSurface;
                } else {
                    // otherwise create a vol surface from puts and strip for a final surface
                    boost::shared_ptr<BlackVarianceSurfaceSparse> putSurface =
                        boost::make_shared<BlackVarianceSurfaceSparse>(asof, calendar_, putExpiries, putStrikes,
                                                                       putData, dayCounter_, flatStrikeExtrap,
                                                                       flatStrikeExtrap, flatTimeExtrap);

                    boost::shared_ptr<EquityOptionSurfaceStripper> eoss =
                        boost::make_shared<EquityOptionSurfaceStripper>(
                            eqIndex, callSurface, putSurface, calendar_, dayCounter_, Exercise::European,
                            flatStrikeExtrap, flatStrikeExtrap, flatTimeExtrap, preferOutOfTheMoney);
                    vol_ = eoss->volSurface();
                }
            }
        } else {
            QL_FAIL("EquityVolatility: Invalid quote type provided.");
        }
        DLOG("Setting BlackVarianceSurfaceSparse extrapolation to " << to_string(vssc.extrapolation()));
        vol_->enableExtrapolation(vssc.extrapolation());

    } catch (std::exception& e) {
        QL_FAIL("equity vol curve building failed :" << e.what());
    } catch (...) {
        QL_FAIL("equity vol curve building failed: unknown error");
    }
}

namespace {
vector<Real> checkMoneyness(const vector<string>& strMoneynessLevels) {

    using boost::adaptors::transformed;
    using boost::algorithm::join;

    vector<Real> moneynessLevels = parseVectorOfValues<Real>(strMoneynessLevels, &parseReal);
    sort(moneynessLevels.begin(), moneynessLevels.end(), [](Real x, Real y) { return !close(x, y) && x < y; });
    QL_REQUIRE(adjacent_find(moneynessLevels.begin(), moneynessLevels.end(),
                             [](Real x, Real y) { return close(x, y); }) == moneynessLevels.end(),
               "The configured moneyness levels contain duplicates");
    DLOG("Parsed " << moneynessLevels.size() << " unique configured moneyness levels.");
    DLOG("The moneyness levels are: " << join(
             moneynessLevels | transformed([](Real d) { return ore::data::to_string(d); }), ","));

    return moneynessLevels;
}
} // namespace

void EquityVolCurve::buildVolatility(const Date& asof, EquityVolatilityCurveConfig& vc,
                                     const VolatilityMoneynessSurfaceConfig& vmsc, const Loader& loader,
                                     const QuantLib::Handle<EquityIndex>& eqIndex) {

    using boost::adaptors::transformed;
    using boost::algorithm::join;

    // Check that the quote type is volatility, we do not support price
    QL_REQUIRE(vc.quoteType() == MarketDatum::QuoteType::RATE_LNVOL,
               "Equity Moneyness Surface supports lognormal volatility quotes only");

    // Parse, sort and check the vector of configured moneyness levels
    vector<Real> moneynessLevels = checkMoneyness(vmsc.moneynessLevels());

    // Expiries may be configured with a wildcard or given explicitly
    bool expWc = false;
    if (find(vmsc.expiries().begin(), vmsc.expiries().end(), "*") != vmsc.expiries().end()) {
        expWc = true;
        QL_REQUIRE(vmsc.expiries().size() == 1, "Wild card expiry specified but more expiries also specified.");
        DLOG("Have expiry wildcard pattern " << vmsc.expiries()[0]);
    }

    // Map to hold the rows of the volatility matrix. The keys are the expiry dates and the values are the
    // vectors of volatilities, one for each configured moneyness.
    map<Date, vector<Real>> surfaceData;

    // Count the number of quotes added. We check at the end that we have added all configured quotes.
    Size quotesAdded = 0;

    // Configured moneyness type.
    MoneynessStrike::Type moneynessType = parseMoneynessType(vmsc.moneynessType());

    // Populate the configured strikes.
    vector<boost::shared_ptr<BaseStrike>> strikes;
    for (const auto& moneynessLevel : moneynessLevels) {
        strikes.push_back(boost::make_shared<MoneynessStrike>(moneynessType, moneynessLevel));
    }

    // Read the quotes to fill the expiry dates and vols matrix.
    for (const boost::shared_ptr<MarketDatum>& md : loader.loadQuotes(asof)) {

        // Go to next quote if the market data point's date does not equal our asof.
        if (md->asofDate() != asof)
            continue;

        // Go to next quote if not a commodity option quote.
        auto q = boost::dynamic_pointer_cast<EquityOptionQuote>(md);
        if (!q)
            continue;

        // Go to next quote if eq name or currency do not match config.
        if (vc.curveID() != q->eqName() || vc.ccy() != q->ccy())
            continue;

        // Go to next quote if quote type does not match config
        if (vc.quoteType() != q->quoteType())
            continue;

        // Iterator to one of the configured strikes.
        vector<boost::shared_ptr<BaseStrike>>::iterator strikeIt;

        if (!expWc) {

            // If we have explicitly configured expiries and the quote is not in the configured quotes continue.
            auto it = find(vc.quotes().begin(), vc.quotes().end(), q->name());
            if (it == vc.quotes().end())
                continue;

            // Check if quote's strike is in the configured strikes, continue if no.
            strikeIt = find_if(strikes.begin(), strikes.end(),
                               [&q](boost::shared_ptr<BaseStrike> s) { return *s == *q->strike(); });
            if (strikeIt == strikes.end())
                continue;

        } else {

            // Check if quote's strike is in the configured strikes and continue if it is not.
            strikeIt = find_if(strikes.begin(), strikes.end(),
                               [&q](boost::shared_ptr<BaseStrike> s) { return *s == *q->strike(); });
            if (strikeIt == strikes.end())
                continue;
        }

        // Position of quote in vector of strikes
        Size pos = std::distance(strikes.begin(), strikeIt);

        // Process the quote
        Date eDate = getDateFromDateOrPeriod(q->expiry(), asof, calendar_);

        // Add quote to surface
        if (surfaceData.count(eDate) == 0)
            surfaceData[eDate] = vector<Real>(moneynessLevels.size(), Null<Real>());

        QL_REQUIRE(surfaceData[eDate][pos] == Null<Real>(),
                   "Quote " << q->name() << " provides a duplicate quote for the date " << io::iso_date(eDate)
                            << " and strike " << *q->strike());
        surfaceData[eDate][pos] = q->quote()->value();
        quotesAdded++;

        TLOG("Added quote " << q->name() << ": (" << io::iso_date(eDate) << "," << *q->strike() << "," << fixed
                            << setprecision(9) << "," << q->quote()->value() << ")");
    }

    LOG("EquityVolCurve: added " << quotesAdded << " quotes in building moneyness strike surface.");

    // Check the data gathered.
    if (!expWc) {
        // If expiries were configured explicitly, the number of configured quotes should equal the
        // number of quotes added.
        QL_REQUIRE(vc.quotes().size() == quotesAdded,
                   "Found " << quotesAdded << " quotes, but " << vc.quotes().size() << " quotes required by config.");
    } else {
        // check we have non-empty surface data
        QL_REQUIRE(!surfaceData.empty(), "Moneyness Surface Data is empty");
        // If the expiries were configured via a wildcard, check that no surfaceData element has a Null<Real>().
        for (const auto& kv : surfaceData) {
            for (Size j = 0; j < moneynessLevels.size(); j++) {
                QL_REQUIRE(kv.second[j] != Null<Real>(), "Volatility for expiry date "
                                                             << io::iso_date(kv.first) << " and strike " << *strikes[j]
                                                             << " not found. Cannot proceed with a sparse matrix.");
            }
        }
    }

    // Populate the volatility quotes and the expiry times.
    // Rows are moneyness levels and columns are expiry times - this is what the ctor needs below.
    vector<Date> expiryDates(surfaceData.size());
    vector<Time> expiryTimes(surfaceData.size());
    vector<vector<Handle<Quote>>> vols(moneynessLevels.size());
    for (const auto row : surfaceData | boost::adaptors::indexed(0)) {
        expiryDates[row.index()] = row.value().first;
        expiryTimes[row.index()] = dayCounter_.yearFraction(asof, row.value().first);
        for (Size i = 0; i < row.value().second.size(); i++) {
            vols[i].push_back(Handle<Quote>(boost::make_shared<SimpleQuote>(row.value().second[i])));
        }
    }

    // Set the strike extrapolation which only matters if extrapolation is turned on for the whole surface.
    // BlackVarianceSurfaceMoneyness time extrapolation is hard-coded to constant in volatility.
    bool flatExtrapolation = true;
    if (vmsc.extrapolation()) {

        auto strikeExtrapType = parseExtrapolation(vmsc.strikeExtrapolation());
        if (strikeExtrapType == Extrapolation::UseInterpolator) {
            DLOG("Strike extrapolation switched to using interpolator.");
            flatExtrapolation = false;
        } else if (strikeExtrapType == Extrapolation::None) {
            DLOG("Strike extrapolation cannot be turned off on its own so defaulting to flat.");
        } else if (strikeExtrapType == Extrapolation::Flat) {
            DLOG("Strike extrapolation has been set to flat.");
        } else {
            DLOG("Strike extrapolation " << strikeExtrapType << " not expected so default to flat.");
        }

        auto timeExtrapType = parseExtrapolation(vmsc.timeExtrapolation());
        if (timeExtrapType != Extrapolation::Flat) {
            DLOG("BlackVarianceSurfaceMoneyness only supports flat volatility extrapolation in the time direction");
        }
    } else {
        DLOG("Extrapolation is turned off for the whole surface so the time and"
             << " strike extrapolation settings are ignored");
    }

    // Time interpolation
    if (vmsc.timeInterpolation() != "Linear") {
        DLOG("BlackVarianceSurfaceMoneyness only supports linear time interpolation in variance.");
    }

    // Strike interpolation
    if (vmsc.strikeInterpolation() != "Linear") {
        DLOG("BlackVarianceSurfaceMoneyness only supports linear strike interpolation in variance.");
    }

    // Both moneyness surfaces need a spot quote.

    // The choice of false here is important for forward moneyness. It means that we use the cpts and yts in the
    // BlackVarianceSurfaceMoneynessForward to get the forward value at all times and in particular at times that
    // are after the last expiry time. If we set it to true, BlackVarianceSurfaceMoneynessForward uses a linear
    // interpolated forward curve on the expiry times internally which is poor.
    bool stickyStrike = false;

    if (moneynessType == MoneynessStrike::Type::Forward) {

        DLOG("Creating BlackVarianceSurfaceMoneynessForward object");
        vol_ = boost::make_shared<BlackVarianceSurfaceMoneynessForward>(
            calendar_, eqIndex->equitySpot(), expiryTimes, moneynessLevels, vols, dayCounter_,
            eqIndex->equityDividendCurve(), eqIndex->equityForecastCurve(), stickyStrike, flatExtrapolation);

    } else {

        DLOG("Creating BlackVarianceSurfaceMoneynessSpot object");
        vol_ = boost::make_shared<BlackVarianceSurfaceMoneynessSpot>(calendar_, eqIndex->equitySpot(), expiryTimes,
                                                                     moneynessLevels, vols, dayCounter_, stickyStrike,
                                                                     flatExtrapolation);
    }

    DLOG("Setting BlackVarianceSurfaceMoneyness extrapolation to " << to_string(vmsc.extrapolation()));
    vol_->enableExtrapolation(vmsc.extrapolation());

    LOG("EquityVolCurve: finished building 2-D volatility moneyness strike surface");
}

void EquityVolCurve::buildVolatility(const QuantLib::Date& asof, EquityVolatilityCurveConfig& vc,
                                     const VolatilityDeltaSurfaceConfig& vdsc, const Loader& loader,
                                     const QuantLib::Handle<QuantExt::EquityIndex>& eqIndex) {

    using boost::adaptors::transformed;
    using boost::algorithm::join;

    LOG("EquityVolCurve: start building 2-D volatility delta strike surface");

    QL_REQUIRE(vc.quoteType() == MarketDatum::QuoteType::RATE_LNVOL,
               "EquityVolCurve: only quote type"
                   << " RATE_LNVOL is currently supported for a 2-D volatility delta strike surface.");

    // Parse, sort and check the vector of configured put deltas
    vector<Real> putDeltas = parseVectorOfValues<Real>(vdsc.putDeltas(), &parseReal);
    sort(putDeltas.begin(), putDeltas.end(), [](Real x, Real y) { return !close(x, y) && x < y; });
    QL_REQUIRE(adjacent_find(putDeltas.begin(), putDeltas.end(), [](Real x, Real y) { return close(x, y); }) ==
                   putDeltas.end(),
               "The configured put deltas contain duplicates");
    DLOG("Parsed " << putDeltas.size() << " unique configured put deltas");
    DLOG("Put deltas are: " << join(putDeltas | transformed([](Real d) { return ore::data::to_string(d); }), ","));

    // Parse, sort descending and check the vector of configured call deltas
    vector<Real> callDeltas = parseVectorOfValues<Real>(vdsc.callDeltas(), &parseReal);
    sort(callDeltas.begin(), callDeltas.end(), [](Real x, Real y) { return !close(x, y) && x > y; });
    QL_REQUIRE(adjacent_find(callDeltas.begin(), callDeltas.end(), [](Real x, Real y) { return close(x, y); }) ==
                   callDeltas.end(),
               "The configured call deltas contain duplicates");
    DLOG("Parsed " << callDeltas.size() << " unique configured call deltas");
    DLOG("Call deltas are: " << join(callDeltas | transformed([](Real d) { return ore::data::to_string(d); }), ","));

    // Expiries may be configured with a wildcard or given explicitly
    bool expWc = false;
    if (find(vdsc.expiries().begin(), vdsc.expiries().end(), "*") != vdsc.expiries().end()) {
        expWc = true;
        QL_REQUIRE(vdsc.expiries().size() == 1, "Wild card expiry specified but more expiries also specified.");
        DLOG("Have expiry wildcard pattern " << vdsc.expiries()[0]);
    }

    // Map to hold the rows of the equity volatility matrix. The keys are the expiry dates and the values are the
    // vectors of volatilities, one for each configured delta.
    map<Date, vector<Real>> surfaceData;

    // Number of strikes = number of put deltas + ATM + number of call deltas
    Size numStrikes = putDeltas.size() + 1 + callDeltas.size();

    // Count the number of quotes added. We check at the end that we have added all configured quotes.
    Size quotesAdded = 0;

    // Configured delta and Atm types.
    DeltaVolQuote::DeltaType deltaType = parseDeltaType(vdsc.deltaType());
    DeltaVolQuote::AtmType atmType = parseAtmType(vdsc.atmType());
    boost::optional<DeltaVolQuote::DeltaType> atmDeltaType;
    if (!vdsc.atmDeltaType().empty()) {
        atmDeltaType = parseDeltaType(vdsc.atmDeltaType());
    }

    // Populate the configured strikes.
    vector<boost::shared_ptr<BaseStrike>> strikes;
    for (const auto& pd : putDeltas) {
        strikes.push_back(boost::make_shared<DeltaStrike>(deltaType, Option::Put, pd));
    }
    strikes.push_back(boost::make_shared<AtmStrike>(atmType, atmDeltaType));
    for (const auto& cd : callDeltas) {
        strikes.push_back(boost::make_shared<DeltaStrike>(deltaType, Option::Call, cd));
    }

    // Read the quotes to fill the expiry dates and vols matrix.
    for (const boost::shared_ptr<MarketDatum>& md : loader.loadQuotes(asof)) {

        // Go to next quote if the market data point's date does not equal our asof.
        if (md->asofDate() != asof)
            continue;

        // Go to next quote if not a commodity option quote.
        auto q = boost::dynamic_pointer_cast<EquityOptionQuote>(md);
        if (!q)
            continue;

        // Go to next quote if not a equity name or currency do not match config.
        if (vc.curveID() != q->eqName() || vc.ccy() != q->ccy())
            continue;

        // Iterator to one of the configured strikes.
        vector<boost::shared_ptr<BaseStrike>>::iterator strikeIt;

        if (!expWc) {

            // If we have explicitly configured expiries and the quote is not in the configured quotes continue.
            auto it = find(vc.quotes().begin(), vc.quotes().end(), q->name());
            if (it == vc.quotes().end())
                continue;

            // Check if quote's strike is in the configured strikes.
            // It should be as we have selected from the explicitly configured quotes in the last step.
            strikeIt = find_if(strikes.begin(), strikes.end(),
                               [&q](boost::shared_ptr<BaseStrike> s) { return *s == *q->strike(); });
            QL_REQUIRE(strikeIt != strikes.end(),
                       "The quote '"
                           << q->name()
                           << "' is in the list of configured quotes but does not match any of the configured strikes");

        } else {

            // Check if quote's strike is in the configured strikes and continue if it is not.
            strikeIt = find_if(strikes.begin(), strikes.end(),
                               [&q](boost::shared_ptr<BaseStrike> s) { return *s == *q->strike(); });
            if (strikeIt == strikes.end())
                continue;
        }

        // Position of quote in vector of strikes
        Size pos = std::distance(strikes.begin(), strikeIt);

        // Process the quote
        Date eDate;
        boost::shared_ptr<Expiry> expiry = parseExpiry(q->expiry());
        if (auto expiryDate = boost::dynamic_pointer_cast<ExpiryDate>(expiry)) {
            eDate = expiryDate->expiryDate();
        } else if (auto expiryPeriod = boost::dynamic_pointer_cast<ExpiryPeriod>(expiry)) {
            // We may need more conventions here eventually.
            eDate = calendar_.adjust(asof + expiryPeriod->expiryPeriod());
        }

        // Add quote to surface
        if (surfaceData.count(eDate) == 0)
            surfaceData[eDate] = vector<Real>(numStrikes, Null<Real>());

        QL_REQUIRE(surfaceData[eDate][pos] == Null<Real>(),
                   "Quote " << q->name() << " provides a duplicate quote for the date " << io::iso_date(eDate)
                            << " and strike " << *q->strike());
        surfaceData[eDate][pos] = q->quote()->value();
        quotesAdded++;

        TLOG("Added quote " << q->name() << ": (" << io::iso_date(eDate) << "," << *q->strike() << "," << fixed
                            << setprecision(9) << "," << q->quote()->value() << ")");
    }

    LOG("EquityVolCurve: added " << quotesAdded << " quotes in building delta strike surface.");

    // Check the data gathered.
    if (!expWc) {
        // If expiries were configured explicitly, the number of configured quotes should equal the
        // number of quotes added.
        QL_REQUIRE(vc.quotes().size() == quotesAdded,
                   "Found " << quotesAdded << " quotes, but " << vc.quotes().size() << " quotes required by config.");
    } else {
        // If the expiries were configured via a wildcard, check that no surfaceData element has a Null<Real>().
        for (const auto& kv : surfaceData) {
            for (Size j = 0; j < numStrikes; j++) {
                QL_REQUIRE(kv.second[j] != Null<Real>(), "Volatility for expiry date "
                                                             << io::iso_date(kv.first) << " and strike " << *strikes[j]
                                                             << " not found. Cannot proceed with a sparse matrix.");
            }
        }
    }

    // Populate the matrix of volatilities and the expiry dates.
    vector<Date> expiryDates;
    Matrix vols(surfaceData.size(), numStrikes);
    for (const auto row : surfaceData | boost::adaptors::indexed(0)) {
        expiryDates.push_back(row.value().first);
        copy(row.value().second.begin(), row.value().second.end(), vols.row_begin(row.index()));
    }

    // Need to multiply each put delta value by -1 before passing it to the BlackVolatilitySurfaceDelta ctor
    // i.e. a put delta of 0.25 that is passed in to the config must be -0.25 when passed to the ctor.
    transform(putDeltas.begin(), putDeltas.end(), putDeltas.begin(), [](Real pd) { return -1.0 * pd; });
    DLOG("Multiply put deltas by -1.0 before creating BlackVolatilitySurfaceDelta object.");
    DLOG("Put deltas are: " << join(putDeltas | transformed([](Real d) { return ore::data::to_string(d); }), ","));

    // Set the strike extrapolation which only matters if extrapolation is turned on for the whole surface.
    // BlackVolatilitySurfaceDelta time extrapolation is hard-coded to constant in volatility.
    bool flatExtrapolation = true;
    if (vdsc.extrapolation()) {

        auto strikeExtrapType = parseExtrapolation(vdsc.strikeExtrapolation());
        if (strikeExtrapType == Extrapolation::UseInterpolator) {
            DLOG("Strike extrapolation switched to using interpolator.");
            flatExtrapolation = false;
        } else if (strikeExtrapType == Extrapolation::None) {
            DLOG("Strike extrapolation cannot be turned off on its own so defaulting to flat.");
        } else if (strikeExtrapType == Extrapolation::Flat) {
            DLOG("Strike extrapolation has been set to flat.");
        } else {
            DLOG("Strike extrapolation " << strikeExtrapType << " not expected so default to flat.");
        }

        auto timeExtrapType = parseExtrapolation(vdsc.timeExtrapolation());
        if (timeExtrapType != Extrapolation::Flat) {
            DLOG("BlackVolatilitySurfaceDelta only supports flat volatility extrapolation in the time direction");
        }
    } else {
        DLOG("Extrapolation is turned off for the whole surface so the time and"
             << " strike extrapolation settings are ignored");
    }

    // Time interpolation
    if (vdsc.timeInterpolation() != "Linear") {
        DLOG("BlackVolatilitySurfaceDelta only supports linear time interpolation.");
    }

    // Strike interpolation
    InterpolatedSmileSection::InterpolationMethod im;
    if (vdsc.strikeInterpolation() == "Linear") {
        im = InterpolatedSmileSection::InterpolationMethod::Linear;
    } else if (vdsc.strikeInterpolation() == "NaturalCubic") {
        im = InterpolatedSmileSection::InterpolationMethod::NaturalCubic;
    } else if (vdsc.strikeInterpolation() == "FinancialCubic") {
        im = InterpolatedSmileSection::InterpolationMethod::FinancialCubic;
    } else {
        im = InterpolatedSmileSection::InterpolationMethod::Linear;
        DLOG("BlackVolatilitySurfaceDelta does not support strike interpolation '" << vdsc.strikeInterpolation()
                                                                                   << "' so setting it to linear.");
    }

    DLOG("Creating BlackVolatilitySurfaceDelta object");
    bool hasAtm = true;
    vol_ = boost::make_shared<BlackVolatilitySurfaceDelta>(
        asof, expiryDates, putDeltas, callDeltas, hasAtm, vols, dayCounter_, calendar_, eqIndex->equitySpot(),
        eqIndex->equityForecastCurve(), eqIndex->equityDividendCurve(), deltaType, atmType, atmDeltaType, 0 * Days,
        deltaType, atmType, atmDeltaType, im, flatExtrapolation);

    DLOG("Setting BlackVolatilitySurfaceDelta extrapolation to " << to_string(vdsc.extrapolation()));
    vol_->enableExtrapolation(vdsc.extrapolation());

    LOG("EquityVolCurve: finished building 2-D volatility delta strike surface");
}

void EquityVolCurve::buildVolatility(const QuantLib::Date& asof, const EquityVolatilityCurveSpec& spec,
                                     const CurveConfigurations& curveConfigs,
                                     const map<string, boost::shared_ptr<EquityCurve>>& eqCurves,
                                     const map<string, boost::shared_ptr<EquityVolCurve>>& eqVolCurves) {

    // get all the configurations and the curve needed for proxying
    auto config = *curveConfigs.equityVolCurveConfig(spec.curveConfigID());

    auto proxy = config.proxySurface();
    auto eqConfig = *curveConfigs.equityCurveConfig(spec.curveConfigID());
    auto proxyConfig = *curveConfigs.equityCurveConfig(proxy);
    auto proxyVolConfig = *curveConfigs.equityVolCurveConfig(proxy);

    // create dummy specs to look up the required curves
    EquityCurveSpec eqSpec(eqConfig.currency(), spec.curveConfigID());
    EquityCurveSpec proxySpec(proxyConfig.currency(), proxy);
    EquityVolatilityCurveSpec proxyVolSpec(proxyVolConfig.ccy(), proxy);

    // Get all necessary curves
    auto curve = eqCurves.find(eqSpec.name());
    QL_REQUIRE(curve != eqCurves.end(), "Failed to find equity curve, when building equity vol curve " << spec.name());
    auto proxyCurve = eqCurves.find(proxySpec.name());
    QL_REQUIRE(proxyCurve != eqCurves.end(), "Failed to find equity curve for proxy "
                                                 << proxySpec.name() << ", when building equity vol curve "
                                                 << spec.name());
    auto proxyVolCurve = eqVolCurves.find(proxyVolSpec.name());
    QL_REQUIRE(proxyVolCurve != eqVolCurves.end(), "Failed to find equity vol curve for proxy "
                                                       << proxyVolSpec.name() << ", when building equity vol curve "
                                                       << spec.name());

    vol_ = boost::make_shared<EquityBlackVolatilitySurfaceProxy>(
        proxyVolCurve->second->volTermStructure(), curve->second->equityIndex(), proxyCurve->second->equityIndex());
}

void EquityVolCurve::buildCalibrationInfo(const QuantLib::Date& asof, const CurveConfigurations& curveConfigs,
                                          const EquityVolatilityCurveConfig& config,
                                          const Handle<EquityIndex>& eqIndex) {

    DLOG("Building calibration info for eq vol surface");

    try {

        ReportConfig rc = effectiveReportConfig(curveConfigs.reportConfigFxVols(), config.reportConfig());

        bool reportOnDeltaGrid = *rc.reportOnDeltaGrid();
        bool reportOnMoneynessGrid = *rc.reportOnMoneynessGrid();
        std::vector<Real> moneyness = *rc.moneyness();
        std::vector<std::string> deltas = *rc.deltas();
        std::vector<Period> expiries = *rc.expiries();

        calibrationInfo_ = boost::make_shared<FxEqVolCalibrationInfo>();

        DeltaVolQuote::AtmType atmType = DeltaVolQuote::AtmType::AtmDeltaNeutral;
        DeltaVolQuote::DeltaType deltaType = DeltaVolQuote::DeltaType::Fwd;

        if (auto vdsc = boost::dynamic_pointer_cast<VolatilityDeltaSurfaceConfig>(config.volatilityConfig())) {
            atmType = parseAtmType(vdsc->atmType());
            deltaType = parseDeltaType(vdsc->deltaType());
        }

        calibrationInfo_->dayCounter = config.dayCounter().empty() ? "na" : config.dayCounter();
        calibrationInfo_->calendar = config.calendar().empty() ? "na" : config.calendar();
        calibrationInfo_->atmType = ore::data::to_string(atmType);
        calibrationInfo_->deltaType = ore::data::to_string(deltaType);
        calibrationInfo_->longTermAtmType = ore::data::to_string(atmType);
        calibrationInfo_->longTermDeltaType = ore::data::to_string(deltaType);
        calibrationInfo_->switchTenor = "na";
        calibrationInfo_->riskReversalInFavorOf = "na";
        calibrationInfo_->butterflyStyle = "na";

        std::vector<Real> times, forwards, rfDisc, divDisc;
        for (auto const& p : expiries) {
            Date d = vol_->optionDateFromTenor(p);
            calibrationInfo_->expiryDates.push_back(d);
            times.push_back(vol_->dayCounter().empty() ? Actual365Fixed().yearFraction(asof, d)
                                                       : vol_->timeFromReference(d));
            forwards.push_back(eqIndex->fixing(d));
            rfDisc.push_back(eqIndex->equityForecastCurve()->discount(d));
            divDisc.push_back(eqIndex->equityDividendCurve()->discount(d));
        }

        calibrationInfo_->times = times;
        calibrationInfo_->forwards = forwards;

        std::vector<std::vector<Real>> callPricesDelta(times.size(), std::vector<Real>(deltas.size(), 0.0));
        std::vector<std::vector<Real>> callPricesMoneyness(times.size(), std::vector<Real>(moneyness.size(), 0.0));

        calibrationInfo_->isArbitrageFree = true;

        if (reportOnDeltaGrid) {
            calibrationInfo_->deltas = deltas;
            calibrationInfo_->deltaGridStrikes =
                std::vector<std::vector<Real>>(times.size(), std::vector<Real>(deltas.size(), 0.0));
            calibrationInfo_->deltaGridProb =
                std::vector<std::vector<Real>>(times.size(), std::vector<Real>(deltas.size(), 0.0));
            calibrationInfo_->deltaGridImpliedVolatility =
                std::vector<std::vector<Real>>(times.size(), std::vector<Real>(deltas.size(), 0.0));
            calibrationInfo_->deltaGridCallSpreadArbitrage =
                std::vector<std::vector<bool>>(times.size(), std::vector<bool>(deltas.size(), true));
            calibrationInfo_->deltaGridButterflyArbitrage =
                std::vector<std::vector<bool>>(times.size(), std::vector<bool>(deltas.size(), true));
            TLOG("Delta surface arbitrage analysis result (no calendar spread arbitrage included):");
            Real maxTime = vol_->timeFromReference(vol_->optionDateFromTenor(expiries.back()));
            DeltaVolQuote::AtmType at;
            DeltaVolQuote::DeltaType dt;
            for (Size i = 0; i < times.size(); ++i) {
                Real t = times[i];
                at = atmType;
                dt = deltaType;
                // for times after the last quoted expiry we use artificial conventions to avoid problems with strike
                // from delta conversions: we use fwd delta always and ATM DNS
                if (t > maxTime) {
                    at = DeltaVolQuote::AtmDeltaNeutral;
                    dt = DeltaVolQuote::Fwd;
                }
                bool validSlice = true;
                for (Size j = 0; j < deltas.size(); ++j) {
                    DeltaString d(deltas[j]);
                    try {
                        Real strike;
                        if (d.isAtm()) {
                            strike = QuantExt::getAtmStrike(dt, at, eqIndex->equitySpot()->value(), rfDisc[i],
                                                            divDisc[i], vol_, t);
                        } else if (d.isCall()) {
                            strike = QuantExt::getStrikeFromDelta(Option::Call, d.delta(), dt,
                                                                  eqIndex->equitySpot()->value(), rfDisc[i], divDisc[i],
                                                                  vol_, t);
                        } else {
                            strike =
                                QuantExt::getStrikeFromDelta(Option::Put, d.delta(), dt, eqIndex->equitySpot()->value(),
                                                             rfDisc[i], divDisc[i], vol_, t);
                        }
                        Real stddev = std::sqrt(vol_->blackVariance(t, strike));
                        callPricesDelta[i][j] = blackFormula(Option::Call, strike, forwards[i], stddev);
                        calibrationInfo_->deltaGridStrikes[i][j] = strike;
                        calibrationInfo_->deltaGridImpliedVolatility[i][j] = stddev / std::sqrt(t);
                    } catch (const std::exception& e) {
                        validSlice = false;
                        TLOG("error for time " << t << " delta " << deltas[j] << ": " << e.what());
                    }
                }
                if (validSlice) {
                    try {
                        QuantExt::CarrMadanMarginalProbability cm(calibrationInfo_->deltaGridStrikes[i], forwards[i],
                                                                  callPricesDelta[i]);
                        calibrationInfo_->deltaGridCallSpreadArbitrage[i] = cm.callSpreadArbitrage();
                        calibrationInfo_->deltaGridButterflyArbitrage[i] = cm.butterflyArbitrage();
                        if (!cm.arbitrageFree())
                            calibrationInfo_->isArbitrageFree = false;
                        calibrationInfo_->deltaGridProb[i] = cm.density();
                        TLOGGERSTREAM << arbitrageAsString(cm);
                    } catch (const std::exception& e) {
                        TLOG("error for time " << t << ": " << e.what());
                        calibrationInfo_->isArbitrageFree = false;
                        TLOGGERSTREAM << "..(invalid slice)..";
                    }
                } else {
                    calibrationInfo_->isArbitrageFree = false;
                    TLOGGERSTREAM << "..(invalid slice)..";
                }
            }
            TLOG("Delta surface arbitrage analysis completed.");
        }

        if (reportOnMoneynessGrid) {
            calibrationInfo_->moneyness = moneyness;
            calibrationInfo_->moneynessGridStrikes =
                std::vector<std::vector<Real>>(times.size(), std::vector<Real>(moneyness.size(), 0.0));
            calibrationInfo_->moneynessGridProb =
                std::vector<std::vector<Real>>(times.size(), std::vector<Real>(moneyness.size(), 0.0));
            calibrationInfo_->moneynessGridImpliedVolatility =
                std::vector<std::vector<Real>>(times.size(), std::vector<Real>(moneyness.size(), 0.0));
            calibrationInfo_->moneynessGridCallSpreadArbitrage =
                std::vector<std::vector<bool>>(times.size(), std::vector<bool>(moneyness.size(), true));
            calibrationInfo_->moneynessGridButterflyArbitrage =
                std::vector<std::vector<bool>>(times.size(), std::vector<bool>(moneyness.size(), true));
            calibrationInfo_->moneynessGridCalendarArbitrage =
                std::vector<std::vector<bool>>(times.size(), std::vector<bool>(moneyness.size(), true));
            for (Size i = 0; i < times.size(); ++i) {
                Real t = times[i];
                for (Size j = 0; j < moneyness.size(); ++j) {
                    try {
                        Real strike = moneyness[j] * forwards[i];
                        calibrationInfo_->moneynessGridStrikes[i][j] = strike;
                        Real stddev = std::sqrt(vol_->blackVariance(t, strike));
                        callPricesMoneyness[i][j] = blackFormula(Option::Call, strike, forwards[i], stddev);
                        calibrationInfo_->moneynessGridImpliedVolatility[i][j] = stddev / std::sqrt(t);
                    } catch (const std::exception& e) {
                        TLOG("error for time " << t << " moneyness " << moneyness[j] << ": " << e.what());
                    }
                }
            }
            if (!times.empty() && !moneyness.empty()) {
                try {
                    QuantExt::CarrMadanSurface cm(times, moneyness, eqIndex->equitySpot()->value(), forwards,
                                                  callPricesMoneyness);
                    for (Size i = 0; i < times.size(); ++i) {
                        calibrationInfo_->moneynessGridProb[i] = cm.timeSlices()[i].density();
                    }
                    calibrationInfo_->moneynessGridCallSpreadArbitrage = cm.callSpreadArbitrage();
                    calibrationInfo_->moneynessGridButterflyArbitrage = cm.butterflyArbitrage();
                    calibrationInfo_->moneynessGridCalendarArbitrage = cm.calendarArbitrage();
                    if (!cm.arbitrageFree())
                        calibrationInfo_->isArbitrageFree = false;
                    TLOG("Moneyness surface Arbitrage analysis result:");
                    TLOGGERSTREAM << arbitrageAsString(cm);
                } catch (const std::exception& e) {
                    TLOG("error: " << e.what());
                    calibrationInfo_->isArbitrageFree = false;
                }
                TLOG("Moneyness surface Arbitrage analysis completed:");
            }
        }

        DLOG("Building calibration info for eq vol surface completed.");

    } catch (std::exception& e) {
        QL_FAIL("eq vol curve calibration info building failed: " << e.what());
    } catch (...) {
        QL_FAIL("eq vol curve calibration info building failed: unknown error");
    }
}

} // namespace data
} // namespace ore
