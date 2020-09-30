/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file ored/model/inflation/infjybuilder.hpp
    \brief Builder for a Jarrow Yildrim inflation model component
    \ingroup models
*/

#pragma once

#include <ored/marketdata/market.hpp>
#include <ored/model/inflation/infjydata.hpp>
#include <ored/model/marketobserver.hpp>
#include <ored/model/modelbuilder.hpp>
#include <qle/models/infjyparameterization.hpp>

namespace ore {
namespace data {

/*! Builder for a Jarrow Yildrim inflation model component
    
    This class is a utility to turn a Jarrow Yildrim inflation model component description into an inflation model 
    parameterization which can be used to instantiate a CrossAssetModel.

    \ingroup models
*/
class InfJyBuilder : public ModelBuilder {
public:
    /*! Constructor
        \param market                   Market object
        \param data                     Jarrow Yildrim inflation model description
        \param configuration            Market configuration to use
        \param referenceCalibrationGrid The reference calibration grid
    */
    InfJyBuilder(
        const boost::shared_ptr<Market>& market,
        const boost::shared_ptr<InfJyData>& data,
        const std::string& configuration = Market::defaultConfiguration,
        const std::string& referenceCalibrationGrid = "");

    using Helpers = std::vector<boost::shared_ptr<QuantLib::CalibrationHelper>>;
    
    //! \name Inspectors
    //@{
    std::string inflationIndex() const;
    boost::shared_ptr<QuantExt::InfJyParameterization> parameterization() const;
    Helpers realRateBasket() const;
    Helpers indexBasket() const;
    //@}

    //! \name ModelBuilder interface
    //@{
    void forceRecalculate() override;
    bool requiresRecalibration() const override;
    //@}

private:
    boost::shared_ptr<Market> market_;
    std::string configuration_;
    boost::shared_ptr<InfJyData> data_;
    std::string referenceCalibrationGrid_;
    
    boost::shared_ptr<QuantExt::InfJyParameterization> parameterization_;
    boost::shared_ptr<MarketObserver> marketObserver_;
    boost::shared_ptr<QuantLib::ZeroInflationIndex> inflationIndex_;
    QuantLib::Handle<QuantLib::CPIVolatilitySurface> inflationVolatility_;

    // Helper flag used in the forceRecalculate() method.
    bool forceCalibration_ = false;

    /*! Calibration instruments to use for calibrating the real rate portion of the JY model. The basket is
        empty if we are not calibrating the real rate portion of the JY model. Depending on the calibration 
        configuration, either the real rate reversion parameter or the real rate volatility parameter will be 
        adjusted in order to match these instruments.
    */
    mutable Helpers realRateBasket_;
    mutable std::vector<bool> rrInstActive_;
    mutable QuantLib::Array rrInstExpiries_;

    /*! Calibration instruments to use for calibrating the inflation index portion of the JY model. The basket is 
        empty if we are not calibrating the inflation index portion of the JY model.
    */
    mutable Helpers indexBasket_;
    mutable std::vector<bool> indexInstActive_;
    mutable QuantLib::Array indexInstExpiries_;

    //! \name LazyObject interface
    //@{
    void performCalculations() const override;
    //@}

    //! Build any calibration baskets requested by the configuration i.e. via the \c data_ member.
    void buildCalibrationBaskets() const;

    //! Build the calibration basket.
    Helpers buildCalibrationBasket(const CalibrationBasket& cb, std::vector<bool>& active,
        QuantLib::Array& expiries) const;

    //! Build a CPI cap floor calibration basket.
    Helpers buildCpiCapFloorBasket(const CalibrationBasket& cb, std::vector<bool>& active,
        QuantLib::Array& expiries) const;

    //! Build a YoY cap floor calibration basket.
    Helpers buildYoYCapFloorBasket(const CalibrationBasket& cb, std::vector<bool>& active,
        QuantLib::Array& expiries) const;

    //! Build a YoY swap calibration basket.
    Helpers buildYoYSwapBasket(const CalibrationBasket& cb, std::vector<bool>& active,
        QuantLib::Array& expiries) const;

    //! Find calibration basket with parameter value equal to \p parameter
    const CalibrationBasket& calibrationBasket(const std::string& parameter) const;

    //! Create the real rate parameterisation.
    boost::shared_ptr<QuantExt::Lgm1fParametrization<ZeroInflationTermStructure>> createRealRateParam() const;

    //! Create the inflation index parameterisation.
    boost::shared_ptr<QuantExt::FxBsParametrization> createIndexParam() const;

    /*! Perform checks and possibly adjust the \p times and \p values array depending on calibration configuration.
    */
    void setupParams(const ModelParameter& param, QuantLib::Array& times, QuantLib::Array& values, 
        const QuantLib::Array& expiries, const std::string& paramName) const;

    //! Create the reference calibration dates.
    std::vector<QuantLib::Date> referenceCalibrationDates() const;
};

}
}
