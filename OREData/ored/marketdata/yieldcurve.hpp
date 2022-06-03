/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 Copyright (C) 2021 Skandinaviska Enskilda Banken AB (publ)
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

/*! \file ored/marketdata/yieldcurve.hpp
    \brief Wrapper class for QuantLib term structures.
    \ingroup curves
*/

#pragma once

#include <ored/configuration/conventions.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/configuration/iborfallbackconfig.hpp>
#include <ored/configuration/yieldcurveconfig.hpp>
#include <ored/marketdata/curvespec.hpp>
#include <ored/marketdata/fxtriangulation.hpp>
#include <ored/marketdata/loader.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/marketdata/todaysmarketcalibrationinfo.hpp>
#include <ored/marketdata/yieldcurve.hpp>

#include <ql/termstructures/yield/ratehelpers.hpp>

namespace ore {
namespace data {
using namespace QuantLib;
using ore::data::Conventions;
using ore::data::CurveConfigurations;
using ore::data::YieldCurveConfig;
using ore::data::YieldCurveConfigMap;
using ore::data::YieldCurveSegment;

class DefaultCurve;
class ReferenceDataManager;

//! Wrapper class for building yield term structures
/*!
  Given yield curve specification and its configuration
  this class will actually build a QuantLib yield
  termstructure.

  \ingroup curves
*/
class YieldCurve {
public:
    //! Supported interpolation variables
    enum class InterpolationVariable { Zero, Discount, Forward };

    //! Supported interpolation methods
    enum class InterpolationMethod {
        Linear,
        LogLinear,
        NaturalCubic,
        FinancialCubic,
        ConvexMonotone,
        Quadratic,
        LogQuadratic,
        Hermite,
        CubicSpline,
        ExponentialSplines, // fitted bond curves only
        NelsonSiegel,       // fitted bond curves only
        Svensson            // fitted bond curves only
    };

    //! Constructor
    YieldCurve( //! Valuation date
        Date asof,
        //! Yield curve specification
        YieldCurveSpec curveSpec,
        //! Repository of yield curve configurations
        const CurveConfigurations& curveConfigs,
        // TODO shared_ptr or ref?
        //! Market data loader
        const Loader& loader,
        //! Map of underlying yield curves if required
        const map<string, boost::shared_ptr<YieldCurve>>& requiredYieldCurves =
            map<string, boost::shared_ptr<YieldCurve>>(),
        //! Map of underlying default curves if required
        const map<string, boost::shared_ptr<DefaultCurve>>& requiredDefaultCurves =
            map<string, boost::shared_ptr<DefaultCurve>>(),
        //! FxTriangultion to get FX rate from cross if needed
        const FXTriangulation& fxTriangulation = FXTriangulation(),
        //! optional pointer to reference data, needed to build fitted bond curves
        const boost::shared_ptr<ReferenceDataManager>& referenceData = nullptr,
        //! ibor fallback config
        const IborFallbackConfig& iborfallbackConfig = IborFallbackConfig::defaultConfig(),
        //! if true keep qloader quotes linked to yield ts, otherwise detach them
        const bool preserveQuoteLinkage = false,
        //! build calibration info
        const bool buildCalibrationInfo = true,
        const Market* market = nullptr);

    //! \name Inspectors
    //@{
    const Handle<YieldTermStructure>& handle() const { return h_; }
    YieldCurveSpec curveSpec() const { return curveSpec_; }
    const Date& asofDate() const { return asofDate_; }
    const Currency& currency() const { return currency_; }
    // might be nullptr, if no info was produced for this curve
    boost::shared_ptr<YieldCurveCalibrationInfo> calibrationInfo() const { return calibrationInfo_; }
    //@}

private:
    Date asofDate_;
    Currency currency_;
    YieldCurveSpec curveSpec_;
    DayCounter zeroDayCounter_;
    bool extrapolation_;
    boost::shared_ptr<YieldCurve> discountCurve_;

    // TODO: const refs for now, only used during ctor
    const Loader& loader_;
    RelinkableHandle<YieldTermStructure> h_;
    boost::shared_ptr<YieldTermStructure> p_;
    boost::shared_ptr<YieldCurveCalibrationInfo> calibrationInfo_;

    void buildDiscountCurve();
    void buildZeroCurve();
    void buildZeroSpreadedCurve();
    void buildBootstrappedCurve();
    //! Build a yield curve that uses QuantExt::DiscountRatioModifiedCurve
    void buildDiscountRatioCurve();
    //! Build a yield curve that uses QuantLib::FittedBondCurve
    void buildFittedBondCurve();
    //! Build a yield curve that uses QuantExt::WeightedYieldTermStructure
    void buildWeightedAverageCurve();
    //! Build a yield curve that uses QuantExt::YieldPlusDefaultYieldTermStructure
    void buildYieldPlusDefaultCurve();
    //! Build a yield curve that uses QuantExt::IborFallbackCurve
    void buildIborFallbackCurve();

    //! Return the yield curve with the given \p id from the requiredYieldCurves_ map
    boost::shared_ptr<YieldCurve> getYieldCurve(const std::string& ccy, const std::string& id) const;

    boost::shared_ptr<YieldCurveConfig> curveConfig_;
    vector<boost::shared_ptr<YieldCurveSegment>> curveSegments_;
    InterpolationVariable interpolationVariable_;
    InterpolationMethod interpolationMethod_;
    map<string, boost::shared_ptr<YieldCurve>> requiredYieldCurves_;
    map<string, boost::shared_ptr<DefaultCurve>> requiredDefaultCurves_;
    const FXTriangulation& fxTriangulation_;
    const boost::shared_ptr<ReferenceDataManager> referenceData_;
    IborFallbackConfig iborFallbackConfig_;
    const bool preserveQuoteLinkage_;
    bool buildCalibrationInfo_;
    const Market* market_;

    boost::shared_ptr<YieldTermStructure> piecewisecurve(vector<boost::shared_ptr<RateHelper>> instruments);

    /* Functions to build RateHelpers from yield curve segments */
    void addDeposits(const boost::shared_ptr<YieldCurveSegment>& segment,
                     vector<boost::shared_ptr<RateHelper>>& instruments);
    void addFutures(const boost::shared_ptr<YieldCurveSegment>& segment,
                    vector<boost::shared_ptr<RateHelper>>& instruments);
    void addFras(const boost::shared_ptr<YieldCurveSegment>& segment,
                 vector<boost::shared_ptr<RateHelper>>& instruments);
    void addOISs(const boost::shared_ptr<YieldCurveSegment>& segment,
                 vector<boost::shared_ptr<RateHelper>>& instruments);
    void addSwaps(const boost::shared_ptr<YieldCurveSegment>& segment,
                  vector<boost::shared_ptr<RateHelper>>& instruments);
    void addAverageOISs(const boost::shared_ptr<YieldCurveSegment>& segment,
                        vector<boost::shared_ptr<RateHelper>>& instruments);
    void addTenorBasisSwaps(const boost::shared_ptr<YieldCurveSegment>& segment,
                            vector<boost::shared_ptr<RateHelper>>& instruments);
    void addTenorBasisTwoSwaps(const boost::shared_ptr<YieldCurveSegment>& segment,
                               vector<boost::shared_ptr<RateHelper>>& instruments);
    void addBMABasisSwaps(const boost::shared_ptr<YieldCurveSegment>& segment,
                          vector<boost::shared_ptr<RateHelper>>& instruments);
    void addFXForwards(const boost::shared_ptr<YieldCurveSegment>& segment,
                       vector<boost::shared_ptr<RateHelper>>& instruments);
    void addCrossCcyBasisSwaps(const boost::shared_ptr<YieldCurveSegment>& segment,
                               vector<boost::shared_ptr<RateHelper>>& instruments);
    void addCrossCcyFixFloatSwaps(const boost::shared_ptr<YieldCurveSegment>& segment,
                                  vector<boost::shared_ptr<RateHelper>>& instruments);

    // get the fx spot from the string provided
    boost::shared_ptr<FXSpotQuote> getFxSpotQuote(string spotId);
};

//! Helper function for parsing interpolation method
YieldCurve::InterpolationMethod parseYieldCurveInterpolationMethod(const string& s);
//! Helper function for parsing interpolation variable
YieldCurve::InterpolationVariable parseYieldCurveInterpolationVariable(const string& s);

//! Templated function to build a YieldTermStructure and apply interpolation methods to it
template <template <class> class CurveType>
boost::shared_ptr<YieldTermStructure> buildYieldCurve(const vector<Date>& dates, const vector<QuantLib::Real>& rates,
                                                      const DayCounter& dayCounter,
                                                      YieldCurve::InterpolationMethod interpolationMethod);

//! Create a Interpolated Zero Curve and apply interpolators
boost::shared_ptr<YieldTermStructure> zerocurve(const vector<Date>& dates, const vector<Rate>& yields,
                                                const DayCounter& dayCounter,
                                                YieldCurve::InterpolationMethod interpolationMethod);

//! Create a Interpolated Discount Curve and apply interpolators
boost::shared_ptr<YieldTermStructure> discountcurve(const vector<Date>& dates, const vector<DiscountFactor>& dfs,
                                                    const DayCounter& dayCounter,
                                                    YieldCurve::InterpolationMethod interpolationMethod);

//! Create a Interpolated Forward Curve and apply interpolators
boost::shared_ptr<YieldTermStructure> forwardcurve(const vector<Date>& dates, const vector<Rate>& forwards,
                                                   const DayCounter& dayCounter,
                                                   YieldCurve::InterpolationMethod interpolationMethod);

} // namespace data
} // namespace ore
