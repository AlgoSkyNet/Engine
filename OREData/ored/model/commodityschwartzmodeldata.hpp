/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

/*! \file model/commodityschwartzmodeldata.hpp
    \brief COM component data for the cross asset model
    \ingroup models
*/

#pragma once

#include <vector>

#include <ql/time/daycounters/actualactual.hpp>
#include <ql/types.hpp>

#include <qle/models/crossassetmodel.hpp>

#include <ored/configuration/conventions.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/model/lgmdata.hpp>
#include <ored/utilities/xmlutils.hpp>

namespace ore {
namespace data {
using namespace QuantLib;

//! COM Schwartz Model Parameters
/*!
  Specification for a COM model component with lognormal forwards in the Cross Asset LGM. 
  This class covers the CommoditySchwartz parametrization.
  \ingroup models
*/
class CommoditySchwartzData {
public:
    //! Default constructor
    CommoditySchwartzData() : driftFreeState_(false) {}

    //! Detailed constructor
    CommoditySchwartzData(std::string name, std::string currency, CalibrationType calibrationType,
                          bool calibrateSigma, Real sigma,
                          bool calibrateKappa, Real kappa,
                          std::vector<std::string> optionExpiries = std::vector<std::string>(),
                          std::vector<std::string> optionStrikes = std::vector<std::string>(),
                          bool driftFreeState = false)
        : name_(name), ccy_(currency), calibrationType_(calibrationType),
          calibrateSigma_(calibrateSigma), sigmaType_(ParamType::Constant), sigmaValue_(sigma),
          calibrateKappa_(calibrateKappa),kappaType_(ParamType::Constant), kappaValue_(kappa),
          optionExpiries_(optionExpiries), optionStrikes_(optionStrikes), driftFreeState_(driftFreeState) {}

    //! \name Setters/Getters
    //@{
    std::string& name() { return name_; }
    std::string& currency() { return ccy_; }
    CalibrationType& calibrationType() { return calibrationType_; }
    bool& calibrateSigma() { return calibrateSigma_; }
    ParamType& sigmaParamType() { return sigmaType_; }
    Real& sigmaValue() { return sigmaValue_; }
    bool& calibrateKappa() { return calibrateKappa_; }
    ParamType& kappaParamType() { return sigmaType_; }
    Real& kappaValue() { return kappaValue_; }
    std::vector<std::string>& optionExpiries() { return optionExpiries_; }
    std::vector<std::string>& optionStrikes() { return optionStrikes_; }
    bool& driftFreeState() { return driftFreeState_; }
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node);
    XMLNode* toXML(XMLDocument& doc);
    //@}

    //! \name Operators
    //@{
    bool operator==(const CommoditySchwartzData& rhs);
    bool operator!=(const CommoditySchwartzData& rhs);
    //@}

private:
    std::string name_;
    std::string ccy_;
    CalibrationType calibrationType_;
    bool calibrateSigma_;
    ParamType sigmaType_;
    Real sigmaValue_;
    bool calibrateKappa_;
    ParamType kappaType_;
    Real kappaValue_;
    std::vector<std::string> optionExpiries_;
    std::vector<std::string> optionStrikes_;
    bool driftFreeState_;
};
} // namespace data
} // namespace ore
