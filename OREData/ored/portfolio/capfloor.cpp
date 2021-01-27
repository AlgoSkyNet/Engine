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

#include <ored/portfolio/builders/capfloor.hpp>
#include <ored/portfolio/builders/cpicapfloor.hpp>
#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/builders/yoycapfloor.hpp>
#include <ored/portfolio/capfloor.hpp>
#include <ored/portfolio/durationadjustedcmslegdata.hpp>
#include <ored/portfolio/fixingdates.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/utilities/log.hpp>

#include <ql/instruments/capfloor.hpp>
#include <ql/instruments/compositeinstrument.hpp>
#include <ql/instruments/cpicapfloor.hpp>
#include <ql/instruments/inflationcapfloor.hpp>
#include <qle/indexes/inflationindexwrapper.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void CapFloor::build(const boost::shared_ptr<EngineFactory>& engineFactory) {

    DLOG("CapFloor::build() called for trade " << id());

    // Make sure the leg is floating or CMS
    QL_REQUIRE((legData_.legType() == "Floating") || (legData_.legType() == "CMS") ||
                   (legData_.legType() == "DurationAdjustedCMS") || (legData_.legType() == "CPI") ||
                   (legData_.legType() == "YY"),
               "CapFloor build error, LegType must be Floating, CMS, DurationAdjustedCMS, CPI or YY");

    // Determine if we have a cap, a floor or a collar
    QL_REQUIRE(caps_.size() > 0 || floors_.size() > 0, "CapFloor build error, no cap rates or floor rates provided");
    QuantLib::CapFloor::Type capFloorType;
    if (floors_.size() == 0) {
        capFloorType = QuantLib::CapFloor::Cap;
    } else if (caps_.size() == 0) {
        capFloorType = QuantLib::CapFloor::Floor;
    } else {
        capFloorType = QuantLib::CapFloor::Collar;
    }

    // Clear legs before building
    legs_.clear();

    boost::shared_ptr<EngineBuilder> builder;
    std::string underlyingIndex, qlIndexName;
    boost::shared_ptr<QuantLib::Instrument> qlInstrument;

    Real multiplier = (parsePositionType(longShort_) == Position::Long ? 1.0 : -1.0);

    DLOG("Building cap/floor on leg of type " << legData_.legType());
    if (legData_.legType() == "Floating") {

        boost::shared_ptr<FloatingLegData> floatData =
            boost::dynamic_pointer_cast<FloatingLegData>(legData_.concreteLegData());
        QL_REQUIRE(floatData, "Wrong LegType, expected Floating, got " << legData_.legType());
        underlyingIndex = floatData->index();
        Handle<IborIndex> hIndex =
            engineFactory->market()->iborIndex(underlyingIndex, engineFactory->configuration(MarketContext::pricing));
        QL_REQUIRE(!hIndex.empty(), "Could not find ibor index " << underlyingIndex << " in market.");
        boost::shared_ptr<IborIndex> index = hIndex.currentLink();
        qlIndexName = index->name();

        QL_REQUIRE(floatData->caps().empty() && floatData->floors().empty(),
                   "CapFloor build error, Floating leg section must not have caps and floors");

        bool isOnIndex = boost::dynamic_pointer_cast<OvernightIndex>(index) != nullptr;
        bool isBmaIndex = boost::dynamic_pointer_cast<QuantExt::BMAIndexWrapper>(index) != nullptr;

        if (!isBmaIndex && !(isOnIndex && floatData->isAveraged()) && !floatData->hasSubPeriods()) {
            // For the cases where we support caps and floors in the regular way, we build a floating leg with
            // the nakedOption flag set to true, this avoids maintaining all features in legs with associated
            // coupon pricers and at the same time in the QuaantLib::CapFloor instrument and pricing engine.
            // Supported cases are
            // - Ibor coupon without sub periods (hasSubPeriods = false)
            // - compounded ON coupon (isAveraged = false)
            // The other cases are handled below.
            LegData tmpLegData = legData_;
            boost::shared_ptr<FloatingLegData> tmpFloatData = boost::make_shared<FloatingLegData>(*floatData);
            tmpFloatData->floors() = floors_;
            tmpFloatData->caps() = caps_;
            tmpFloatData->nakedOption() = true;
            tmpLegData.concreteLegData() = tmpFloatData;
            legs_.push_back(engineFactory->legBuilder(tmpLegData.legType())
                                ->buildLeg(tmpLegData, engineFactory, requiredFixings_,
                                           engineFactory->configuration(MarketContext::pricing)));
            // if both caps and floors are given, we have to use a payer leg, since in this case
            // the StrippedCappedFlooredCoupon used to extract the naked options assumes a long floor
            // and a short cap while we have documented a collar to be a short floor and long cap
            qlInstrument =
                boost::make_shared<QuantLib::Swap>(legs_, std::vector<bool>{!floors_.empty() && !caps_.empty()});
            qlInstrument->setPricingEngine(
                boost::make_shared<DiscountingSwapEngine>(engineFactory->market()->discountCurve(legData_.currency())));
            maturity_ = CashFlows::maturityDate(legs_.front());
        } else {
            // For the cases where we don't have regular cap / floor support we treat the index approximately as an Ibor
            // index and build an QuantLib::CapFloor with associated pricing engine. These cases comprise:
            // - BMA coupons
            // - Ibor coupons with sub periods (hasSubPeriods = true)
            // - averaged ON coupons (isAveraged = true)
            ALOG("CapFloor trade " << id()
                                   << " on a) BMA or b) sub periods Ibor or c) averaged ON underlying (index = '"
                                   << underlyingIndex
                                   << "') built, will treat the index approximately as an ibor index");
            builder = engineFactory->builder(tradeType_);
            legs_.push_back(makeIborLeg(legData_, index, engineFactory));

            // If a vector of cap/floor rates are provided, ensure they align with the number of schedule periods
            if (floors_.size() > 1) {
                QL_REQUIRE(floors_.size() == legs_[0].size(),
                           "The number of floor rates provided does not match the number of schedule periods");
            }

            if (caps_.size() > 1) {
                QL_REQUIRE(caps_.size() == legs_[0].size(),
                           "The number of cap rates provided does not match the number of schedule periods");
            }

            // If one cap/floor rate is given, extend the vector to align with the number of schedule periods
            if (floors_.size() == 1)
                floors_.resize(legs_[0].size(), floors_[0]);

            if (caps_.size() == 1)
                caps_.resize(legs_[0].size(), caps_[0]);

            // Create QL CapFloor instrument
            qlInstrument = boost::make_shared<QuantLib::CapFloor>(capFloorType, legs_[0], caps_, floors_);

            boost::shared_ptr<CapFloorEngineBuilder> capFloorBuilder =
                boost::dynamic_pointer_cast<CapFloorEngineBuilder>(builder);
            qlInstrument->setPricingEngine(capFloorBuilder->engine(parseCurrency(legData_.currency())));

            maturity_ = boost::dynamic_pointer_cast<QuantLib::CapFloor>(qlInstrument)->maturityDate();
        }

    } else if (legData_.legType() == "CMS") {
        builder = engineFactory->builder("Swap");

        boost::shared_ptr<CMSLegData> cmsData = boost::dynamic_pointer_cast<CMSLegData>(legData_.concreteLegData());
        QL_REQUIRE(cmsData, "Wrong LegType, expected CMS");

        underlyingIndex = cmsData->swapIndex();
        Handle<SwapIndex> hIndex =
            engineFactory->market()->swapIndex(underlyingIndex, builder->configuration(MarketContext::pricing));
        QL_REQUIRE(!hIndex.empty(), "Could not find swap index " << underlyingIndex << " in market.");

        boost::shared_ptr<SwapIndex> index = hIndex.currentLink();
        qlIndexName = index->name();

        bool payer = (parsePositionType(longShort_) == Position::Long ? false : true);
        vector<bool> legPayers_;
        legs_.push_back(makeCMSLeg(legData_, index, engineFactory, caps_, floors_));
        legPayers_.push_back(!payer);
        legs_.push_back(makeCMSLeg(legData_, index, engineFactory));
        legPayers_.push_back(payer);

        qlInstrument = boost::make_shared<QuantLib::Swap>(legs_, legPayers_);
        boost::shared_ptr<SwapEngineBuilderBase> cmsCapFloorBuilder =
            boost::dynamic_pointer_cast<SwapEngineBuilderBase>(builder);
        qlInstrument->setPricingEngine(cmsCapFloorBuilder->engine(parseCurrency(legData_.currency())));

        maturity_ = boost::dynamic_pointer_cast<QuantLib::Swap>(qlInstrument)->maturityDate();

    } else if (legData_.legType() == "DurationAdjustedCMS") {
        auto cmsData = boost::dynamic_pointer_cast<DurationAdjustedCmsLegData>(legData_.concreteLegData());
        QL_REQUIRE(cmsData, "Wrong LegType, expected DurationAdjustedCmsLegData");
        LegData tmpLegData = legData_;
        auto tmpCmsData = boost::make_shared<DurationAdjustedCmsLegData>(*cmsData);
        tmpCmsData->floors() = floors_;
        tmpCmsData->caps() = caps_;
        tmpCmsData->nakedOption() = true;
        tmpLegData.concreteLegData() = tmpCmsData;
        legs_.push_back(engineFactory->legBuilder(tmpLegData.legType())
                            ->buildLeg(tmpLegData, engineFactory, requiredFixings_,
                                       engineFactory->configuration(MarketContext::pricing)));
        // if both caps and floors are given, we have to use a payer leg, since in this case
        // the StrippedCappedFlooredCoupon used to extract the naked options assumes a long floor
        // and a short cap while we have documented a collar to be a short floor and long cap
        qlInstrument = boost::make_shared<QuantLib::Swap>(legs_, std::vector<bool>{!floors_.empty() && !caps_.empty()});
        qlInstrument->setPricingEngine(
            boost::make_shared<DiscountingSwapEngine>(engineFactory->market()->discountCurve(legData_.currency())));
        maturity_ = CashFlows::maturityDate(legs_.front());
    } else if (legData_.legType() == "CPI") {
        DLOG("CPI CapFloor Type " << capFloorType << " ID " << id());

        builder = engineFactory->builder("CpiCapFloor");

        boost::shared_ptr<CPILegData> cpiData = boost::dynamic_pointer_cast<CPILegData>(legData_.concreteLegData());
        QL_REQUIRE(cpiData, "Wrong LegType, expected CPI");

        underlyingIndex = cpiData->index();
        Handle<ZeroInflationIndex> zeroIndex = engineFactory->market()->zeroInflationIndex(
            underlyingIndex, builder->configuration(MarketContext::pricing));
        qlIndexName = zeroIndex->name();

        // the cpi leg uses the first schedule date as the start date, which only makes sense if there are at least
        // two dates in the schedule, otherwise the only date in the schedule is the pay date of the cf and a a separate
        // start date is expected; if both the separate start date and a schedule with more than one date is given
        Date startDate;
        Schedule schedule = makeSchedule(legData_.schedule());

        if (schedule.size() < 2) {
            QL_REQUIRE(!cpiData->startDate().empty(),
                       "makeCPILeg(): if only one schedule date is given, a StartDate must be given in addition");
            startDate = parseDate(cpiData->startDate());
        } else {
            QL_REQUIRE(cpiData->startDate().empty() || parseDate(cpiData->startDate()) == schedule.dates().front(),
                       "makeCPILeg(): first schedule date ("
                           << schedule.dates().front() << ") must be identical to start date ("
                           << parseDate(cpiData->startDate())
                           << "), the start date can be omitted for schedules containing more than one date");
            startDate = schedule.dates().front();
        }

        Real baseCPI = cpiData->baseCPI();
        Period observationLag = parsePeriod(cpiData->observationLag());
        CPI::InterpolationType interpolation = parseObservationInterpolation(cpiData->interpolation());
        Calendar cal = zeroIndex->fixingCalendar();
        BusinessDayConvention conv = Unadjusted; // not used in the CPI CapFloor engine

        QL_REQUIRE(!zeroIndex.empty(), "Zero Inflation Index is empty");

        legs_.push_back(makeCPILeg(legData_, zeroIndex.currentLink(), engineFactory));

        // If a vector of cap/floor rates are provided, ensure they align with the number of schedule periods
        if (floors_.size() > 1) {
            QL_REQUIRE(floors_.size() == legs_[0].size(),
                       "The number of floor rates provided does not match the number of schedule periods");
        }

        if (caps_.size() > 1) {
            QL_REQUIRE(caps_.size() == legs_[0].size(),
                       "The number of cap rates provided does not match the number of schedule periods");
        }

        // If one cap/floor rate is given, extend the vector to align with the number of schedule periods
        if (floors_.size() == 1)
            floors_.resize(legs_[0].size(), floors_[0]);

        if (caps_.size() == 1)
            caps_.resize(legs_[0].size(), caps_[0]);

        boost::shared_ptr<CpiCapFloorEngineBuilder> capFloorBuilder =
            boost::dynamic_pointer_cast<CpiCapFloorEngineBuilder>(builder);

        // Create QL CPI CapFloor instruments and add to a composite
        qlInstrument = boost::make_shared<CompositeInstrument>();
        bool legIsPayer = legData_.isPayer();
        maturity_ = Date::minDate();
        for (Size i = 0; i < legs_[0].size(); ++i) {
            DLOG("Create composite " << i);
            Real nominal, gearing, gearingSign;
            Date paymentDate;
            boost::shared_ptr<CPICoupon> coupon = boost::dynamic_pointer_cast<CPICoupon>(legs_[0][i]);
            boost::shared_ptr<CPICashFlow> cashflow = boost::dynamic_pointer_cast<CPICashFlow>(legs_[0][i]);
            if (coupon) {
                nominal = coupon->nominal();
                gearing = coupon->fixedRate() * coupon->accrualPeriod();
                gearingSign = gearing >= 0.0 ? 1.0 : -1.0;
                paymentDate = coupon->date();
            } else if (cashflow) {
                nominal = cashflow->notional();
                gearing = 1.0; // no gearing here
                gearingSign = 1.0;
                paymentDate = cashflow->date();
            } else {
                QL_FAIL("Failed to interprete CPI flow");
            }

            if (capFloorType == QuantLib::CapFloor::Cap || capFloorType == QuantLib::CapFloor::Collar) {
                Option::Type type = legIsPayer ? Option::Put : Option::Call;
                // long call, short put, consistent with IR and YOY caps/floors/collars
                Real sign = type == Option::Call ? gearingSign : -gearingSign;
                boost::shared_ptr<CPICapFloor> capfloor =
                    boost::make_shared<CPICapFloor>(type, nominal, startDate, baseCPI, paymentDate, cal, conv, cal,
                                                    conv, caps_[i], zeroIndex, observationLag, interpolation);
                capfloor->setPricingEngine(capFloorBuilder->engine(underlyingIndex));
                boost::dynamic_pointer_cast<QuantLib::CompositeInstrument>(qlInstrument)->add(capfloor, sign * gearing);
                // DLOG(id() << " CPI CapFloor Component " << i << " NPV " << capfloor->NPV() << " " << type
                //                                << " sign*gearing=" << sign * gearing);
                maturity_ = std::max(maturity_, capfloor->payDate());
                // if (coupon) {
                //   std::cout << "CapFloor CPI Coupon " << std::endl
                // 	    << "  payment date = " << QuantLib::io::iso_date(paymentDate) << std::endl
                // 	    << "  nominal = " << nominal << std::endl
                // 	    << "  gearing = " << gearing << std::endl
                // 	    << "  start date = " << startDate << std::endl
                // 	    << "  baseCPI = " << baseCPI << std::endl
                // 	    << "  index  = " << zeroIndex->name() << std::endl
                // 	    << "  lag = " << observationLag << std::endl
                // 	    << "  interpolation = " << interpolation << std::endl;
                //   }
            }

            if (capFloorType == QuantLib::CapFloor::Floor || capFloorType == QuantLib::CapFloor::Collar) {
                Option::Type type = legIsPayer ? Option::Call : Option::Put;
                // long call, short put, consistent with IR and YOY caps/floors/collars
                Real sign = type == Option::Call ? gearingSign : -gearingSign;
                boost::shared_ptr<CPICapFloor> capfloor =
                    boost::make_shared<CPICapFloor>(type, nominal, startDate, baseCPI, paymentDate, cal, conv, cal,
                                                    conv, floors_[i], zeroIndex, observationLag, interpolation);
                capfloor->setPricingEngine(capFloorBuilder->engine(underlyingIndex));
                boost::dynamic_pointer_cast<QuantLib::CompositeInstrument>(qlInstrument)->add(capfloor, sign * gearing);
                // DLOG(id() << " CPI CapFloor Component " << i << " NPV " << capfloor->NPV() << " " << type
                //                                << " sign*gearing=" << sign * gearing);
                maturity_ = std::max(maturity_, capfloor->payDate());
            }

            // DLOG(id() << " CPI CapFloor Composite NPV " << composite->NPV());
        }

    } else if (legData_.legType() == "YY") {
        builder = engineFactory->builder("YYCapFloor");

        boost::shared_ptr<YoYLegData> yyData = boost::dynamic_pointer_cast<YoYLegData>(legData_.concreteLegData());
        QL_REQUIRE(yyData, "Wrong LegType, expected YY");

        underlyingIndex = yyData->index();
        Handle<YoYInflationIndex> yoyIndex;
        // look for yoy inflation index
        yoyIndex =
            engineFactory->market()->yoyInflationIndex(underlyingIndex, builder->configuration(MarketContext::pricing));
        qlIndexName = yoyIndex->name();

        // we must have either an yoy or a zero inflation index in the market, if no yoy curve, get teh zero
        // and create a yoy index from it
        if (yoyIndex.empty()) {
            Handle<ZeroInflationIndex> zeroIndex = engineFactory->market()->zeroInflationIndex(
                underlyingIndex, builder->configuration(MarketContext::pricing));
            QL_REQUIRE(!zeroIndex.empty(), "Could not find inflation index (of type either zero or yoy) "
                                               << underlyingIndex << " in market.");
            yoyIndex = Handle<YoYInflationIndex>(boost::make_shared<QuantExt::YoYInflationIndexWrapper>(
                zeroIndex.currentLink(), zeroIndex->interpolated()));
        }

        legs_.push_back(makeYoYLeg(legData_, yoyIndex.currentLink(), engineFactory));

        // If a vector of cap/floor rates are provided, ensure they align with the number of schedule periods
        if (floors_.size() > 1) {
            QL_REQUIRE(floors_.size() == legs_[0].size(),
                       "The number of floor rates provided does not match the number of schedule periods");
        }

        if (caps_.size() > 1) {
            QL_REQUIRE(caps_.size() == legs_[0].size(),
                       "The number of cap rates provided does not match the number of schedule periods");
        }

        // If one cap/floor rate is given, extend the vector to align with the number of schedule periods
        if (floors_.size() == 1)
            floors_.resize(legs_[0].size(), floors_[0]);

        if (caps_.size() == 1)
            caps_.resize(legs_[0].size(), caps_[0]);

        // Create QL YoY Inflation CapFloor instrument
        if (capFloorType == QuantLib::CapFloor::Cap) {
            qlInstrument = boost::shared_ptr<YoYInflationCapFloor>(new YoYInflationCap(legs_[0], caps_));
        } else if (capFloorType == QuantLib::CapFloor::Floor) {
            qlInstrument = boost::shared_ptr<YoYInflationCapFloor>(new YoYInflationFloor(legs_[0], floors_));
        } else if (capFloorType == QuantLib::CapFloor::Collar) {
            qlInstrument = boost::shared_ptr<YoYInflationCapFloor>(
                new YoYInflationCapFloor(QuantLib::YoYInflationCapFloor::Collar, legs_[0], caps_, floors_));
        } else {
            QL_FAIL("unknown YoYInflation cap/floor type");
        }

        boost::shared_ptr<YoYCapFloorEngineBuilder> capFloorBuilder =
            boost::dynamic_pointer_cast<YoYCapFloorEngineBuilder>(builder);
        qlInstrument->setPricingEngine(capFloorBuilder->engine(underlyingIndex));

        // Wrap the QL instrument in a vanilla instrument

        maturity_ = boost::dynamic_pointer_cast<QuantLib::YoYInflationCapFloor>(qlInstrument)->maturityDate();
    } else {
        QL_FAIL("Invalid legType for CapFloor");
    }

    // If premium data is provided
    // 1) build the fee trade and pass it to the instrument wrapper for pricing
    // 2) add fee payment as additional trade leg for cash flow reporting
    std::vector<boost::shared_ptr<Instrument>> additionalInstruments;
    std::vector<Real> additionalMultipliers;
    QL_REQUIRE((premiumPayDate_.empty() && premiumCcy_.empty() && premium_ == Null<Real>()) ||
                   (!premiumPayDate_.empty() && !premiumCcy_.empty() && premium_ != Null<Real>()),
               "CapFloorBuilder: incomplete premium data, expect PremiumAmount, PremiumCurrency, PremiumPayDate");
    if (premiumPayDate_ != "" && premiumCcy_ != "" && premium_ != Null<Real>()) {
        Real premiumAmount = -multiplier * premium_; // pay if long, receive if short
        Currency premiumCurrency = parseCurrency(premiumCcy_);
        Date premiumDate = parseDate(premiumPayDate_);
        addPayment(additionalInstruments, additionalMultipliers, 1.0, premiumDate, premiumAmount, premiumCurrency,
                   parseCurrency(legData_.currency()), engineFactory,
                   engineFactory->configuration(MarketContext::pricing));
        DLOG("option premium added for cap/floor " << id());
    }

    // set instrument
    instrument_ =
        boost::make_shared<VanillaInstrument>(qlInstrument, multiplier, additionalInstruments, additionalMultipliers);

    // add required fixings
    if (!qlIndexName.empty() && !underlyingIndex.empty()) {
        auto fdg = boost::make_shared<FixingDateGetter>(requiredFixings_,
                                                        std::map<string, string>{{qlIndexName, underlyingIndex}});
        for (auto const& l : legs_)
            addToRequiredFixings(l, fdg);
    }

    // Fill in remaining Trade member data
    legCurrencies_.push_back(legData_.currency());
    legPayers_.push_back(legData_.isPayer());
    npvCurrency_ = legData_.currency();
    notionalCurrency_ = legData_.currency();
    notional_ = currentNotional(legs_[0]);
}

void CapFloor::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* capFloorNode = XMLUtils::getChildNode(node, "CapFloorData");
    longShort_ = XMLUtils::getChildValue(capFloorNode, "LongShort", true);
    legData_.fromXML(XMLUtils::getChildNode(capFloorNode, "LegData"));
    caps_ = XMLUtils::getChildrenValuesAsDoubles(capFloorNode, "Caps", "Cap");
    floors_ = XMLUtils::getChildrenValuesAsDoubles(capFloorNode, "Floors", "Floor");
    if (auto c = XMLUtils::getChildNode(capFloorNode, "PremiumAmount")) {
        premium_ = parseReal(XMLUtils::getNodeValue(c));
    } else {
        premium_ = Null<Real>();
    }
    premiumCcy_ = XMLUtils::getChildValue(capFloorNode, "PremiumCurrency", false);
    premiumPayDate_ = XMLUtils::getChildValue(capFloorNode, "PremiumPayDate", false);
}

XMLNode* CapFloor::toXML(XMLDocument& doc) {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* capFloorNode = doc.allocNode("CapFloorData");
    XMLUtils::appendNode(node, capFloorNode);
    XMLUtils::addChild(doc, capFloorNode, "LongShort", longShort_);
    XMLUtils::appendNode(capFloorNode, legData_.toXML(doc));
    XMLUtils::addChildren(doc, capFloorNode, "Caps", "Cap", caps_);
    XMLUtils::addChildren(doc, capFloorNode, "Floors", "Floor", floors_);
    if (!premiumCcy_.empty() && !premiumPayDate_.empty() && premium_ != Null<Real>()) {
        XMLUtils::addChild(doc, capFloorNode, "PremiumAmount", premium_);
        XMLUtils::addChild(doc, capFloorNode, "PremiumCurrency", premiumCcy_);
        XMLUtils::addChild(doc, capFloorNode, "PremiumPayDate", premiumPayDate_);
    }
    return node;
}
} // namespace data
} // namespace ore
