/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
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

#include <orea/simm/simmconcentration.hpp>
#include <orea/simm/simmconfigurationisdav1_0.hpp>
#include <ored/utilities/parsers.hpp>

#include <boost/make_shared.hpp>

#include <ql/math/matrix.hpp>

using ore::data::parseInteger;
using QuantLib::Integer;
using QuantLib::Matrix;
using QuantLib::Real;
using std::string;
using std::vector;

namespace ore {
namespace analytics {

SimmConfiguration_ISDA_V1_0::SimmConfiguration_ISDA_V1_0(const QuantLib::ext::shared_ptr<SimmBucketMapper>& simmBucketMapper,
                                                         const std::string& name, const std::string version)
    : SimmConfigurationBase(simmBucketMapper, name, version) {

    // Set up the correct concentration threshold getter
    simmConcentration_ = QuantLib::ext::make_shared<SimmConcentrationBase>();

    // clang-format off

    // Set up the members for this configuration
    // Explanations of all these members are given in the hpp file
    
    mapBuckets_ = { 
        { CrifRecord::RiskType::IRCurve, { "1", "2", "3" } },
        { CrifRecord::RiskType::CreditQ, { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "Residual" } },
        { CrifRecord::RiskType::CreditVol, { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "Residual" } },
        { CrifRecord::RiskType::CreditNonQ, { "1", "2", "Residual" } },
        { CrifRecord::RiskType::CreditVolNonQ, { "1", "2", "Residual" } },
        { CrifRecord::RiskType::Equity, { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "Residual" } },
        { CrifRecord::RiskType::EquityVol, { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "Residual" } },
        { CrifRecord::RiskType::Commodity, { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16" } },
        { CrifRecord::RiskType::CommodityVol, { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16" } }
    };

    mapLabels_1_ = {
        { CrifRecord::RiskType::IRCurve, { "2w", "1m", "3m", "6m", "1y", "2y", "3y", "5y", "10y", "15y", "20y", "30y" } },
        { CrifRecord::RiskType::CreditQ, { "1y", "2y", "3y", "5y", "10y" } }
    };
    mapLabels_1_[CrifRecord::RiskType::IRVol] = mapLabels_1_[CrifRecord::RiskType::IRCurve];
    mapLabels_1_[CrifRecord::RiskType::EquityVol] = mapLabels_1_[CrifRecord::RiskType::IRCurve];
    mapLabels_1_[CrifRecord::RiskType::CommodityVol] = mapLabels_1_[CrifRecord::RiskType::IRCurve];
    mapLabels_1_[CrifRecord::RiskType::FXVol] = mapLabels_1_[CrifRecord::RiskType::IRCurve];
    mapLabels_1_[CrifRecord::RiskType::CreditNonQ] = mapLabels_1_[CrifRecord::RiskType::CreditQ];
    mapLabels_1_[CrifRecord::RiskType::CreditVol] = mapLabels_1_[CrifRecord::RiskType::CreditQ];
    mapLabels_1_[CrifRecord::RiskType::CreditVolNonQ] = mapLabels_1_[CrifRecord::RiskType::CreditQ];

    mapLabels_2_ = {
        { CrifRecord::RiskType::IRCurve, { "OIS", "Libor1m", "Libor3m", "Libor6m", "Libor12m", "Prime" } },
        { CrifRecord::RiskType::CreditQ, { "", "Sec" } }
    };

    // Risk weights
    rwRiskType_ = {
        { CrifRecord::RiskType::Inflation, 32 },
        { CrifRecord::RiskType::IRVol, 0.21 },
        { CrifRecord::RiskType::CreditVol, 0.35 },
        { CrifRecord::RiskType::CreditVolNonQ, 0.35 },
        { CrifRecord::RiskType::EquityVol, 0.21 },
        { CrifRecord::RiskType::CommodityVol, 0.36 },
        { CrifRecord::RiskType::FX, 7.9 },
        { CrifRecord::RiskType::FXVol, 0.21 },
        { CrifRecord::RiskType::BaseCorr, 18.0 }
    };

    rwBucket_ = {{CrifRecord::RiskType::CreditQ,
                  {{{"1", "", ""}, 97.0},
                   {{"2", "", ""}, 110.0},
                   {{"3", "", ""}, 73.0},
                   {{"4", "", ""}, 65.0},
                   {{"5", "", ""}, 52.0},
                   {{"6", "", ""}, 39.0},
                   {{"7", "", ""}, 198.0},
                   {{"8", "", ""}, 638.0},
                   {{"9", "", ""}, 210.0},
                   {{"10", "", ""}, 375.0},
                   {{"11", "", ""}, 240.0},
                   {{"12", "", ""}, 152.0},
                   {{"Residual", "", ""}, 638.0}}},
                 {CrifRecord::RiskType::CreditNonQ,
                  {{{"1", "", ""}, 169.0},
                   {{"2", "", ""}, 1646.0},
                   {{"Residual", "", ""}, 1646.0}}},
                 {CrifRecord::RiskType::Equity,
                  {{{"1", "", ""}, 22.0},
                   {{"2", "", ""}, 28.0},
                   {{"3", "", ""}, 28.0},
                   {{"4", "", ""}, 25.0},
                   {{"5", "", ""}, 18.0},
                   {{"6", "", ""}, 20.0},
                   {{"7", "", ""}, 24.0},
                   {{"8", "", ""}, 23.0},
                   {{"9", "", ""}, 26.0},
                   {{"10", "", ""}, 27.0},
                   {{"11", "", ""}, 15.0},
                   {{"Residual", "", ""}, 28.0}}},
                 {CrifRecord::RiskType::Commodity,
                  {{{"1", "", ""}, 9.0},
                   {{"2", "", ""}, 19.0},
                   {{"3", "", ""}, 18.0},
                   {{"4", "", ""}, 13.0},
                   {{"5", "", ""}, 24.0},
                   {{"6", "", ""}, 17.0},
                   {{"7", "", ""}, 21.0},
                   {{"8", "", ""}, 35.0},
                   {{"9", "", ""}, 20.0},
                   {{"10", "", ""}, 50.0},
                   {{"11", "", ""}, 21.0},
                   {{"12", "", ""}, 19.0},
                   {{"13", "", ""}, 17.0},
                   {{"14", "", ""}, 15.0},
                   {{"15", "", ""}, 8.0},
                   {{"16", "", ""}, 50.0}}}
    };

    rwLabel_1_ = {
        {CrifRecord::RiskType::IRCurve,
         {{{"1", "2w", ""}, 77.0},
          {{"1", "1m", ""}, 77.0},
          {{"1", "3m", ""}, 77.0},
          {{"1", "6m", ""}, 64.0},
          {{"1", "1y", ""}, 58.0},
          {{"1", "2y", ""}, 49.0},
          {{"1", "3y", ""}, 47.0},
          {{"1", "5y", ""}, 47.0},
          {{"1", "10y", ""}, 45.0},
          {{"1", "15y", ""}, 45.0},
          {{"1", "20y", ""}, 48.0},
          {{"1", "30y", ""}, 56.0},
          {{"2", "2w", ""}, 10.0},
          {{"2", "1m", ""}, 10.0},
          {{"2", "3m", ""}, 10.0},
          {{"2", "6m", ""}, 10.0},
          {{"2", "1y", ""}, 13.0},
          {{"2", "2y", ""}, 16.0},
          {{"2", "3y", ""}, 18.0},
          {{"2", "5y", ""}, 20.0},
          {{"2", "10y", ""}, 25.0},
          {{"2", "15y", ""}, 22.0},
          {{"2", "20y", ""}, 22.0},
          {{"2", "30y", ""}, 23.0},
          {{"3", "2w", ""}, 89.0},
          {{"3", "1m", ""}, 89.0},
          {{"3", "3m", ""}, 89.0},
          {{"3", "6m", ""}, 94.0},
          {{"3", "1y", ""}, 104.0},
          {{"3", "2y", ""}, 99.0},
          {{"3", "3y", ""}, 96.0},
          {{"3", "5y", ""}, 99.0},
          {{"3", "10y", ""}, 87.0},
          {{"3", "15y", ""}, 97.0},
          {{"3", "20y", ""}, 97.0},
          {{"3", "30y", ""}, 98.0}}}
    };

    // Curvature weights
    curvatureWeights_ = {
        { CrifRecord::RiskType::IRVol, { 0.5, 
                             0.5 * 14.0 / (365.0 / 12.0), 
                             0.5 * 14.0 / (3.0 * 365.0 / 12.0), 
                             0.5 * 14.0 / (6.0 * 365.0 / 12.0), 
                             0.5 * 14.0 / 365.0, 
                             0.5 * 14.0 / (2.0 * 365.0), 
                             0.5 * 14.0 / (3.0 * 365.0), 
                             0.5 * 14.0 / (5.0 * 365.0), 
                             0.5 * 14.0 / (10.0 * 365.0), 
                             0.5 * 14.0 / (15.0 * 365.0), 
                             0.5 * 14.0 / (20.0 * 365.0), 
                             0.5 * 14.0 / (30.0 * 365.0) } 
        },
        { CrifRecord::RiskType::CreditVol, { 0.5 * 14.0 / 365.0, 
                                 0.5 * 14.0 / (2.0 * 365.0), 
                                 0.5 * 14.0 / (3.0 * 365.0), 
                                 0.5 * 14.0 / (5.0 * 365.0), 
                                 0.5 * 14.0 / (10.0 * 365.0) } 
        }
    };
    curvatureWeights_[CrifRecord::RiskType::EquityVol] = curvatureWeights_[CrifRecord::RiskType::IRVol];
    curvatureWeights_[CrifRecord::RiskType::CommodityVol] = curvatureWeights_[CrifRecord::RiskType::IRVol];
    curvatureWeights_[CrifRecord::RiskType::FXVol] = curvatureWeights_[CrifRecord::RiskType::IRVol];
    curvatureWeights_[CrifRecord::RiskType::CreditVolNonQ] = curvatureWeights_[CrifRecord::RiskType::CreditVol];

    // Historical volatility ratios empty (1.0 for everything)

    // Valid risk types
    validRiskTypes_ = {
        CrifRecord::RiskType::Commodity,
        CrifRecord::RiskType::CommodityVol,
        CrifRecord::RiskType::CreditNonQ,
        CrifRecord::RiskType::CreditQ,
        CrifRecord::RiskType::CreditVol,
        CrifRecord::RiskType::CreditVolNonQ,
        CrifRecord::RiskType::Equity,
        CrifRecord::RiskType::EquityVol,
        CrifRecord::RiskType::FX,
        CrifRecord::RiskType::FXVol,
        CrifRecord::RiskType::Inflation,
        CrifRecord::RiskType::IRCurve,
        CrifRecord::RiskType::IRVol
    };

    // Risk class correlation matrix
    riskClassCorrelation_ = {
        {{"", "InterestRate", "CreditQualifying"}, 0.09},
        {{"", "InterestRate", "CreditNonQualifying"}, 0.1},
        {{"", "InterestRate", "Equity"}, 0.18},
        {{"", "InterestRate", "Commodity"}, 0.32},
        {{"", "InterestRate", "FX"}, 0.27},
        {{"", "CreditQualifying", "InterestRate"}, 0.09},
        {{"", "CreditQualifying", "CreditNonQualifying"}, 0.24},
        {{"", "CreditQualifying", "Equity"}, 0.58},
        {{"", "CreditQualifying", "Commodity"}, 0.34},
        {{"", "CreditQualifying", "FX"}, 0.29},
        {{"", "CreditNonQualifying", "InterestRate"}, 0.1},
        {{"", "CreditNonQualifying", "CreditQualifying"}, 0.24},
        {{"", "CreditNonQualifying", "Equity"}, 0.23},
        {{"", "CreditNonQualifying", "Commodity"}, 0.24},
        {{"", "CreditNonQualifying", "FX"}, 0.12},
        {{"", "Equity", "InterestRate"}, 0.18},
        {{"", "Equity", "CreditQualifying"}, 0.58},
        {{"", "Equity", "CreditNonQualifying"}, 0.23},
        {{"", "Equity", "Commodity"}, 0.26},
        {{"", "Equity", "FX"}, 0.31},
        {{"", "Commodity", "InterestRate"}, 0.32},
        {{"", "Commodity", "CreditQualifying"}, 0.34},
        {{"", "Commodity", "CreditNonQualifying"}, 0.24},
        {{"", "Commodity", "Equity"}, 0.26},
        {{"", "Commodity", "FX"}, 0.37},
        {{"", "FX", "InterestRate"}, 0.27},
        {{"", "FX", "CreditQualifying"}, 0.29},
        {{"", "FX", "CreditNonQualifying"}, 0.12},
        {{"", "FX", "Equity"}, 0.31},
        {{"", "FX", "Commodity"}, 0.37}
    };

    // Interest rate tenor correlations (i.e. Label1 level correlations)
    intraBucketCorrelation_[CrifRecord::RiskType::IRCurve] = {
        {{"", "2w", "1m"}, 1.0},     
        {{"", "2w", "3m"}, 1.0},     
        {{"", "2w", "6m"}, 0.782},
        {{"", "2w", "1y"}, 0.618},   
        {{"", "2w", "2y"}, 0.498},   
        {{"", "2w", "3y"}, 0.438},
        {{"", "2w", "5y"}, 0.361},   
        {{"", "2w", "10y"}, 0.27},   
        {{"", "2w", "15y"}, 0.196},
        {{"", "2w", "20y"}, 0.174},  
        {{"", "2w", "30y"}, 0.129},  
        {{"", "1m", "2w"}, 1.0},
        {{"", "1m", "3m"}, 1.0},     
        {{"", "1m", "6m"}, 0.782},   
        {{"", "1m", "1y"}, 0.618},
        {{"", "1m", "2y"}, 0.498},   
        {{"", "1m", "3y"}, 0.438},   
        {{"", "1m", "5y"}, 0.361},
        {{"", "1m", "10y"}, 0.27},   
        {{"", "1m", "15y"}, 0.196},  
        {{"", "1m", "20y"}, 0.174},
        {{"", "1m", "30y"}, 0.129},  
        {{"", "3m", "2w"}, 1.0},     
        {{"", "3m", "1m"}, 1.0},
        {{"", "3m", "6m"}, 0.782},   
        {{"", "3m", "1y"}, 0.618},   
        {{"", "3m", "2y"}, 0.498},
        {{"", "3m", "3y"}, 0.438},   
        {{"", "3m", "5y"}, 0.361},   
        {{"", "3m", "10y"}, 0.27},
        {{"", "3m", "15y"}, 0.196},  
        {{"", "3m", "20y"}, 0.174},  
        {{"", "3m", "30y"}, 0.129},
        {{"", "6m", "2w"}, 0.782},   
        {{"", "6m", "1m"}, 0.782},   
        {{"", "6m", "3m"}, 0.782},
        {{"", "6m", "1y"}, 0.84},    
        {{"", "6m", "2y"}, 0.739},   
        {{"", "6m", "3y"}, 0.667},
        {{"", "6m", "5y"}, 0.569},   
        {{"", "6m", "10y"}, 0.444},  
        {{"", "6m", "15y"}, 0.375},
        {{"", "6m", "20y"}, 0.349},  
        {{"", "6m", "30y"}, 0.296},  
        {{"", "1y", "2w"}, 0.618},
        {{"", "1y", "1m"}, 0.618},   
        {{"", "1y", "3m"}, 0.618},   
        {{"", "1y", "6m"}, 0.84},
        {{"", "1y", "2y"}, 0.917},   
        {{"", "1y", "3y"}, 0.859},   
        {{"", "1y", "5y"}, 0.757},
        {{"", "1y", "10y"}, 0.626},  
        {{"", "1y", "15y"}, 0.555},  
        {{"", "1y", "20y"}, 0.526},
        {{"", "1y", "30y"}, 0.471},  
        {{"", "2y", "2w"}, 0.498},   
        {{"", "2y", "1m"}, 0.498},
        {{"", "2y", "3m"}, 0.498},   
        {{"", "2y", "6m"}, 0.739},   
        {{"", "2y", "1y"}, 0.917},
        {{"", "2y", "3y"}, 0.976},   
        {{"", "2y", "5y"}, 0.895},   
        {{"", "2y", "10y"}, 0.749},
        {{"", "2y", "15y"}, 0.69},   
        {{"", "2y", "20y"}, 0.66},   
        {{"", "2y", "30y"}, 0.602},
        {{"", "3y", "2w"}, 0.438},   
        {{"", "3y", "1m"}, 0.438},   
        {{"", "3y", "3m"}, 0.438},
        {{"", "3y", "6m"}, 0.667},   
        {{"", "3y", "1y"}, 0.859},   
        {{"", "3y", "2y"}, 0.976},
        {{"", "3y", "5y"}, 0.958},   
        {{"", "3y", "10y"}, 0.831},  
        {{"", "3y", "15y"}, 0.779},
        {{"", "3y", "20y"}, 0.746},  
        {{"", "3y", "30y"}, 0.69},   
        {{"", "5y", "2w"}, 0.361},
        {{"", "5y", "1m"}, 0.361},   
        {{"", "5y", "3m"}, 0.361},   
        {{"", "5y", "6m"}, 0.569},
        {{"", "5y", "1y"}, 0.757},   
        {{"", "5y", "2y"}, 0.895},   
        {{"", "5y", "3y"}, 0.958},
        {{"", "5y", "10y"}, 0.925},  
        {{"", "5y", "15y"}, 0.893},  
        {{"", "5y", "20y"}, 0.859},
        {{"", "5y", "30y"}, 0.812},  
        {{"", "10y", "2w"}, 0.27},   
        {{"", "10y", "1m"}, 0.27},
        {{"", "10y", "3m"}, 0.27},   
        {{"", "10y", "6m"}, 0.444},  
        {{"", "10y", "1y"}, 0.626},
        {{"", "10y", "2y"}, 0.749},  
        {{"", "10y", "3y"}, 0.831},  
        {{"", "10y", "5y"}, 0.925},
        {{"", "10y", "15y"}, 0.98},  
        {{"", "10y", "20y"}, 0.961}, 
        {{"", "10y", "30y"}, 0.931},
        {{"", "15y", "2w"}, 0.196},  
        {{"", "15y", "1m"}, 0.196},  
        {{"", "15y", "3m"}, 0.196},
        {{"", "15y", "6m"}, 0.375},  
        {{"", "15y", "1y"}, 0.555},  
        {{"", "15y", "2y"}, 0.69},
        {{"", "15y", "3y"}, 0.779},  
        {{"", "15y", "5y"}, 0.893},  
        {{"", "15y", "10y"}, 0.98},
        {{"", "15y", "20y"}, 0.989}, 
        {{"", "15y", "30y"}, 0.97},  
        {{"", "20y", "2w"}, 0.174},
        {{"", "20y", "1m"}, 0.174},  
        {{"", "20y", "3m"}, 0.174},  
        {{"", "20y", "6m"}, 0.349},
        {{"", "20y", "1y"}, 0.526},  
        {{"", "20y", "2y"}, 0.66},   
        {{"", "20y", "3y"}, 0.746},
        {{"", "20y", "5y"}, 0.859},  
        {{"", "20y", "10y"}, 0.961}, 
        {{"", "20y", "15y"}, 0.989},
        {{"", "20y", "30y"}, 0.988}, 
        {{"", "30y", "2w"}, 0.129},  
        {{"", "30y", "1m"}, 0.129},
        {{"", "30y", "3m"}, 0.129},  
        {{"", "30y", "6m"}, 0.296},  
        {{"", "30y", "1y"}, 0.471},
        {{"", "30y", "2y"}, 0.602},  
        {{"", "30y", "3y"}, 0.69},   
        {{"", "30y", "5y"}, 0.812},
        {{"", "30y", "10y"}, 0.931}, 
        {{"", "30y", "15y"}, 0.97},  
        {{"", "30y", "20y"}, 0.988}
    };

    // CreditQ inter-bucket correlations
    interBucketCorrelation_[CrifRecord::RiskType::CreditQ] = {
        {{"", "1", "2"}, 0.51},
        {{"", "1", "3"}, 0.47},
        {{"", "1", "4"}, 0.49},
        {{"", "1", "5"}, 0.46},
        {{"", "1", "6"}, 0.47},
        {{"", "1", "7"}, 0.41},
        {{"", "1", "8"}, 0.36},
        {{"", "1", "9"}, 0.45},
        {{"", "1", "10"}, 0.47},
        {{"", "1", "11"}, 0.47},
        {{"", "1", "12"}, 0.43},
        {{"", "2", "1"}, 0.51},
        {{"", "2", "3"}, 0.52},
        {{"", "2", "4"}, 0.52},
        {{"", "2", "5"}, 0.49},
        {{"", "2", "6"}, 0.52},
        {{"", "2", "7"}, 0.37},
        {{"", "2", "8"}, 0.41},
        {{"", "2", "9"}, 0.51},
        {{"", "2", "10"}, 0.5},
        {{"", "2", "11"}, 0.51},
        {{"", "2", "12"}, 0.46},
        {{"", "3", "1"}, 0.47},
        {{"", "3", "2"}, 0.52},
        {{"", "3", "4"}, 0.54},
        {{"", "3", "5"}, 0.51},
        {{"", "3", "6"}, 0.55},
        {{"", "3", "7"}, 0.37},
        {{"", "3", "8"}, 0.37},
        {{"", "3", "9"}, 0.51},
        {{"", "3", "10"}, 0.49},
        {{"", "3", "11"}, 0.5},
        {{"", "3", "12"}, 0.47},
        {{"", "4", "1"}, 0.49},
        {{"", "4", "2"}, 0.52},
        {{"", "4", "3"}, 0.54},
        {{"", "4", "5"}, 0.53},
        {{"", "4", "6"}, 0.56},
        {{"", "4", "7"}, 0.36},
        {{"", "4", "8"}, 0.37},
        {{"", "4", "9"}, 0.52},
        {{"", "4", "10"}, 0.51},
        {{"", "4", "11"}, 0.51},
        {{"", "4", "12"}, 0.46},
        {{"", "5", "1"}, 0.46},
        {{"", "5", "2"}, 0.49},
        {{"", "5", "3"}, 0.51},
        {{"", "5", "4"}, 0.53},
        {{"", "5", "6"}, 0.54},
        {{"", "5", "7"}, 0.35},
        {{"", "5", "8"}, 0.35},
        {{"", "5", "9"}, 0.49},
        {{"", "5", "10"}, 0.48},
        {{"", "5", "11"}, 0.5},
        {{"", "5", "12"}, 0.44},
        {{"", "6", "1"}, 0.47},
        {{"", "6", "2"}, 0.52},
        {{"", "6", "3"}, 0.55},
        {{"", "6", "4"}, 0.56},
        {{"", "6", "5"}, 0.54},
        {{"", "6", "7"}, 0.37},
        {{"", "6", "8"}, 0.37},
        {{"", "6", "9"}, 0.52},
        {{"", "6", "10"}, 0.49},
        {{"", "6", "11"}, 0.51},
        {{"", "6", "12"}, 0.48},
        {{"", "7", "1"}, 0.41},
        {{"", "7", "2"}, 0.37},
        {{"", "7", "3"}, 0.37},
        {{"", "7", "4"}, 0.36},
        {{"", "7", "5"}, 0.35},
        {{"", "7", "6"}, 0.37},
        {{"", "7", "8"}, 0.29},
        {{"", "7", "9"}, 0.36},
        {{"", "7", "10"}, 0.34},
        {{"", "7", "11"}, 0.36},
        {{"", "7", "12"}, 0.36},
        {{"", "8", "1"}, 0.36},
        {{"", "8", "2"}, 0.41},
        {{"", "8", "3"}, 0.37},
        {{"", "8", "4"}, 0.37},
        {{"", "8", "5"}, 0.35},
        {{"", "8", "6"}, 0.37},
        {{"", "8", "7"}, 0.29},
        {{"", "8", "9"}, 0.37},
        {{"", "8", "10"}, 0.36},
        {{"", "8", "11"}, 0.37},
        {{"", "8", "12"}, 0.33},
        {{"", "9", "1"}, 0.45},
        {{"", "9", "2"}, 0.51},
        {{"", "9", "3"}, 0.51},
        {{"", "9", "4"}, 0.52},
        {{"", "9", "5"}, 0.49},
        {{"", "9", "6"}, 0.52},
        {{"", "9", "7"}, 0.36},
        {{"", "9", "8"}, 0.37},
        {{"", "9", "10"}, 0.49},
        {{"", "9", "11"}, 0.5},
        {{"", "9", "12"}, 0.46},
        {{"", "10", "1"}, 0.47},
        {{"", "10", "2"}, 0.5},
        {{"", "10", "3"}, 0.49},
        {{"", "10", "4"}, 0.51},
        {{"", "10", "5"}, 0.48},
        {{"", "10", "6"}, 0.49},
        {{"", "10", "7"}, 0.34},
        {{"", "10", "8"}, 0.36},
        {{"", "10", "9"}, 0.49},
        {{"", "10", "11"}, 0.49},
        {{"", "10", "12"}, 0.46},
        {{"", "11", "1"}, 0.47},
        {{"", "11", "2"}, 0.51},
        {{"", "11", "3"}, 0.5},
        {{"", "11", "4"}, 0.51},
        {{"", "11", "5"}, 0.5},
        {{"", "11", "6"}, 0.51},
        {{"", "11", "7"}, 0.36},
        {{"", "11", "8"}, 0.37},
        {{"", "11", "9"}, 0.5},
        {{"", "11", "10"}, 0.49},
        {{"", "11", "12"}, 0.46},
        {{"", "12", "1"}, 0.43},
        {{"", "12", "2"}, 0.46},
        {{"", "12", "3"}, 0.47},
        {{"", "12", "4"}, 0.46},
        {{"", "12", "5"}, 0.44},
        {{"", "12", "6"}, 0.48},
        {{"", "12", "7"}, 0.36},
        {{"", "12", "8"}, 0.33},
        {{"", "12", "9"}, 0.46},
        {{"", "12", "10"}, 0.46},
        {{"", "12", "11"}, 0.46}
    };

    // Equity inter-bucket correlations
    interBucketCorrelation_[CrifRecord::RiskType::Equity] = {
        {{"", "1", "2"}, 0.17},
        {{"", "1", "3"}, 0.18},
        {{"", "1", "4"}, 0.16},
        {{"", "1", "5"}, 0.08},
        {{"", "1", "6"}, 0.1},
        {{"", "1", "7"}, 0.1},
        {{"", "1", "8"}, 0.11},
        {{"", "1", "9"}, 0.16},
        {{"", "1", "10"}, 0.08},
        {{"", "1", "11"}, 0.18},
        {{"", "2", "1"}, 0.17},
        {{"", "2", "3"}, 0.24},
        {{"", "2", "4"}, 0.19},
        {{"", "2", "5"}, 0.07},
        {{"", "2", "6"}, 0.1},
        {{"", "2", "7"}, 0.09},
        {{"", "2", "8"}, 0.1},
        {{"", "2", "9"}, 0.19},
        {{"", "2", "10"}, 0.07},
        {{"", "2", "11"}, 0.18},
        {{"", "3", "1"}, 0.18},
        {{"", "3", "2"}, 0.24},
        {{"", "3", "4"}, 0.21},
        {{"", "3", "5"}, 0.09},
        {{"", "3", "6"}, 0.12},
        {{"", "3", "7"}, 0.13},
        {{"", "3", "8"}, 0.13},
        {{"", "3", "9"}, 0.2},
        {{"", "3", "10"}, 0.1},
        {{"", "3", "11"}, 0.24},
        {{"", "4", "1"}, 0.16},
        {{"", "4", "2"}, 0.19},
        {{"", "4", "3"}, 0.21},
        {{"", "4", "5"}, 0.13},
        {{"", "4", "6"}, 0.17},
        {{"", "4", "7"}, 0.16},
        {{"", "4", "8"}, 0.17},
        {{"", "4", "9"}, 0.2},
        {{"", "4", "10"}, 0.13},
        {{"", "4", "11"}, 0.3},
        {{"", "5", "1"}, 0.08},
        {{"", "5", "2"}, 0.07},
        {{"", "5", "3"}, 0.09},
        {{"", "5", "4"}, 0.13},
        {{"", "5", "6"}, 0.28},
        {{"", "5", "7"}, 0.24},
        {{"", "5", "8"}, 0.28},
        {{"", "5", "9"}, 0.1},
        {{"", "5", "10"}, 0.23},
        {{"", "5", "11"}, 0.38},
        {{"", "6", "1"}, 0.1},
        {{"", "6", "2"}, 0.1},
        {{"", "6", "3"}, 0.12},
        {{"", "6", "4"}, 0.17},
        {{"", "6", "5"}, 0.28},
        {{"", "6", "7"}, 0.3},
        {{"", "6", "8"}, 0.33},
        {{"", "6", "9"}, 0.13},
        {{"", "6", "10"}, 0.26},
        {{"", "6", "11"}, 0.45},
        {{"", "7", "1"}, 0.1},
        {{"", "7", "2"}, 0.09},
        {{"", "7", "3"}, 0.13},
        {{"", "7", "4"}, 0.16},
        {{"", "7", "5"}, 0.24},
        {{"", "7", "6"}, 0.3},
        {{"", "7", "8"}, 0.29},
        {{"", "7", "9"}, 0.13},
        {{"", "7", "10"}, 0.25},
        {{"", "7", "11"}, 0.42},
        {{"", "8", "1"}, 0.11},
        {{"", "8", "2"}, 0.1},
        {{"", "8", "3"}, 0.13},
        {{"", "8", "4"}, 0.17},
        {{"", "8", "5"}, 0.28},
        {{"", "8", "6"}, 0.33},
        {{"", "8", "7"}, 0.29},
        {{"", "8", "9"}, 0.14},
        {{"", "8", "10"}, 0.27},
        {{"", "8", "11"}, 0.45},
        {{"", "9", "1"}, 0.16},
        {{"", "9", "2"}, 0.19},
        {{"", "9", "3"}, 0.2},
        {{"", "9", "4"}, 0.2},
        {{"", "9", "5"}, 0.1},
        {{"", "9", "6"}, 0.13},
        {{"", "9", "7"}, 0.13},
        {{"", "9", "8"}, 0.14},
        {{"", "9", "10"}, 0.11},
        {{"", "9", "11"}, 0.25},
        {{"", "10", "1"}, 0.08},
        {{"", "10", "2"}, 0.07},
        {{"", "10", "3"}, 0.1},
        {{"", "10", "4"}, 0.13},
        {{"", "10", "5"}, 0.23},
        {{"", "10", "6"}, 0.26},
        {{"", "10", "7"}, 0.25},
        {{"", "10", "8"}, 0.27},
        {{"", "10", "9"}, 0.11},
        {{"", "10", "11"}, 0.34},
        {{"", "11", "1"}, 0.18},
        {{"", "11", "2"}, 0.18},
        {{"", "11", "3"}, 0.24},
        {{"", "11", "4"}, 0.3},
        {{"", "11", "5"}, 0.38},
        {{"", "11", "6"}, 0.45},
        {{"", "11", "7"}, 0.42},
        {{"", "11", "8"}, 0.45},
        {{"", "11", "9"}, 0.25},
        {{"", "11", "10"}, 0.34}
    };

    // Commodity inter-bucket correlations
    interBucketCorrelation_[CrifRecord::RiskType::Commodity] = {
        {{"", "1", "2"}, 0.11},
        {{"", "1", "3"}, 0.16},
        {{"", "1", "4"}, 0.13},
        {{"", "1", "5"}, 0.1},
        {{"", "1", "6"}, 0.06},
        {{"", "1", "7"}, 0.2},
        {{"", "1", "8"}, 0.05},
        {{"", "1", "9"}, 0.17},
        {{"", "1", "10"}, 0.03},
        {{"", "1", "11"}, 0.18},
        {{"", "1", "12"}, 0.09},
        {{"", "1", "13"}, 0.1},
        {{"", "1", "14"}, 0.05},
        {{"", "1", "15"}, 0.04},
        {{"", "1", "16"}, 0.0},
        {{"", "2", "1"}, 0.11},
        {{"", "2", "3"}, 0.95},
        {{"", "2", "4"}, 0.95},
        {{"", "2", "5"}, 0.93},
        {{"", "2", "6"}, 0.15},
        {{"", "2", "7"}, 0.27},
        {{"", "2", "8"}, 0.19},
        {{"", "2", "9"}, 0.2},
        {{"", "2", "10"}, 0.14},
        {{"", "2", "11"}, 0.3},
        {{"", "2", "12"}, 0.31},
        {{"", "2", "13"}, 0.26},
        {{"", "2", "14"}, 0.26},
        {{"", "2", "15"}, 0.12},
        {{"", "2", "16"}, 0.0},
        {{"", "3", "1"}, 0.16},
        {{"", "3", "2"}, 0.95},
        {{"", "3", "4"}, 0.92},
        {{"", "3", "5"}, 0.9},
        {{"", "3", "6"}, 0.17},
        {{"", "3", "7"}, 0.24},
        {{"", "3", "8"}, 0.14},
        {{"", "3", "9"}, 0.17},
        {{"", "3", "10"}, 0.12},
        {{"", "3", "11"}, 0.32},
        {{"", "3", "12"}, 0.26},
        {{"", "3", "13"}, 0.16},
        {{"", "3", "14"}, 0.22},
        {{"", "3", "15"}, 0.12},
        {{"", "3", "16"}, 0.0},
        {{"", "4", "1"}, 0.13},
        {{"", "4", "2"}, 0.95},
        {{"", "4", "3"}, 0.92},
        {{"", "4", "5"}, 0.9},
        {{"", "4", "6"}, 0.18},
        {{"", "4", "7"}, 0.26},
        {{"", "4", "8"}, 0.08},
        {{"", "4", "9"}, 0.17},
        {{"", "4", "10"}, 0.08},
        {{"", "4", "11"}, 0.31},
        {{"", "4", "12"}, 0.25},
        {{"", "4", "13"}, 0.15},
        {{"", "4", "14"}, 0.2},
        {{"", "4", "15"}, 0.09},
        {{"", "4", "16"}, 0.0},
        {{"", "5", "1"}, 0.1},
        {{"", "5", "2"}, 0.93},
        {{"", "5", "3"}, 0.9},
        {{"", "5", "4"}, 0.9},
        {{"", "5", "6"}, 0.18},
        {{"", "5", "7"}, 0.37},
        {{"", "5", "8"}, 0.13},
        {{"", "5", "9"}, 0.3},
        {{"", "5", "10"}, 0.21},
        {{"", "5", "11"}, 0.34},
        {{"", "5", "12"}, 0.32},
        {{"", "5", "13"}, 0.27},
        {{"", "5", "14"}, 0.29},
        {{"", "5", "15"}, 0.12},
        {{"", "5", "16"}, 0.0},
        {{"", "6", "1"}, 0.06},
        {{"", "6", "2"}, 0.15},
        {{"", "6", "3"}, 0.17},
        {{"", "6", "4"}, 0.18},
        {{"", "6", "5"}, 0.18},
        {{"", "6", "7"}, 0.07},
        {{"", "6", "8"}, 0.62},
        {{"", "6", "9"}, 0.03},
        {{"", "6", "10"}, 0.15},
        {{"", "6", "11"}, 0.0},
        {{"", "6", "12"}, 0.0},
        {{"", "6", "13"}, 0.23},
        {{"", "6", "14"}, 0.15},
        {{"", "6", "15"}, 0.07},
        {{"", "6", "16"}, 0.0},
        {{"", "7", "1"}, 0.2},
        {{"", "7", "2"}, 0.27},
        {{"", "7", "3"}, 0.24},
        {{"", "7", "4"}, 0.26},
        {{"", "7", "5"}, 0.37},
        {{"", "7", "6"}, 0.07},
        {{"", "7", "8"}, 0.07},
        {{"", "7", "9"}, 0.66},
        {{"", "7", "10"}, 0.2},
        {{"", "7", "11"}, 0.06},
        {{"", "7", "12"}, 0.06},
        {{"", "7", "13"}, 0.12},
        {{"", "7", "14"}, 0.09},
        {{"", "7", "15"}, 0.09},
        {{"", "7", "16"}, 0.0},
        {{"", "8", "1"}, 0.05},
        {{"", "8", "2"}, 0.19},
        {{"", "8", "3"}, 0.14},
        {{"", "8", "4"}, 0.08},
        {{"", "8", "5"}, 0.13},
        {{"", "8", "6"}, 0.62},
        {{"", "8", "7"}, 0.07},
        {{"", "8", "9"}, 0.09},
        {{"", "8", "10"}, 0.12},
        {{"", "8", "11"}, -0.01},
        {{"", "8", "12"}, 0.0},
        {{"", "8", "13"}, 0.18},
        {{"", "8", "14"}, 0.11},
        {{"", "8", "15"}, 0.04},
        {{"", "8", "16"}, 0.0},
        {{"", "9", "1"}, 0.17},
        {{"", "9", "2"}, 0.2},
        {{"", "9", "3"}, 0.17},
        {{"", "9", "4"}, 0.17},
        {{"", "9", "5"}, 0.3},
        {{"", "9", "6"}, 0.03},
        {{"", "9", "7"}, 0.66},
        {{"", "9", "8"}, 0.09},
        {{"", "9", "10"}, 0.12},
        {{"", "9", "11"}, 0.1},
        {{"", "9", "12"}, 0.06},
        {{"", "9", "13"}, 0.12},
        {{"", "9", "14"}, 0.1},
        {{"", "9", "15"}, 0.1},
        {{"", "9", "16"}, 0.0},
        {{"", "10", "1"}, 0.03},
        {{"", "10", "2"}, 0.14},
        {{"", "10", "3"}, 0.12},
        {{"", "10", "4"}, 0.08},
        {{"", "10", "5"}, 0.21},
        {{"", "10", "6"}, 0.15},
        {{"", "10", "7"}, 0.2},
        {{"", "10", "8"}, 0.12},
        {{"", "10", "9"}, 0.12},
        {{"", "10", "11"}, 0.1},
        {{"", "10", "12"}, 0.07},
        {{"", "10", "13"}, 0.09},
        {{"", "10", "14"}, 0.1},
        {{"", "10", "15"}, 0.16},
        {{"", "10", "16"}, 0.0},
        {{"", "11", "1"}, 0.18},
        {{"", "11", "2"}, 0.3},
        {{"", "11", "3"}, 0.32},
        {{"", "11", "4"}, 0.31},
        {{"", "11", "5"}, 0.34},
        {{"", "11", "6"}, 0.0},
        {{"", "11", "7"}, 0.06},
        {{"", "11", "8"}, -0.01},
        {{"", "11", "9"}, 0.1},
        {{"", "11", "10"}, 0.1},
        {{"", "11", "12"}, 0.46},
        {{"", "11", "13"}, 0.2},
        {{"", "11", "14"}, 0.26},
        {{"", "11", "15"}, 0.18},
        {{"", "11", "16"}, 0.0},
        {{"", "12", "1"}, 0.09},
        {{"", "12", "2"}, 0.31},
        {{"", "12", "3"}, 0.26},
        {{"", "12", "4"}, 0.25},
        {{"", "12", "5"}, 0.32},
        {{"", "12", "6"}, 0.0},
        {{"", "12", "7"}, 0.06},
        {{"", "12", "8"}, 0.0},
        {{"", "12", "9"}, 0.06},
        {{"", "12", "10"}, 0.07},
        {{"", "12", "11"}, 0.46},
        {{"", "12", "13"}, 0.25},
        {{"", "12", "14"}, 0.23},
        {{"", "12", "15"}, 0.14},
        {{"", "12", "16"}, 0.0},
        {{"", "13", "1"}, 0.1},
        {{"", "13", "2"}, 0.26},
        {{"", "13", "3"}, 0.16},
        {{"", "13", "4"}, 0.15},
        {{"", "13", "5"}, 0.27},
        {{"", "13", "6"}, 0.23},
        {{"", "13", "7"}, 0.12},
        {{"", "13", "8"}, 0.18},
        {{"", "13", "9"}, 0.12},
        {{"", "13", "10"}, 0.09},
        {{"", "13", "11"}, 0.2},
        {{"", "13", "12"}, 0.25},
        {{"", "13", "14"}, 0.29},
        {{"", "13", "15"}, 0.06},
        {{"", "13", "16"}, 0.0},
        {{"", "14", "1"}, 0.05},
        {{"", "14", "2"}, 0.26},
        {{"", "14", "3"}, 0.22},
        {{"", "14", "4"}, 0.2},
        {{"", "14", "5"}, 0.29},
        {{"", "14", "6"}, 0.15},
        {{"", "14", "7"}, 0.09},
        {{"", "14", "8"}, 0.11},
        {{"", "14", "9"}, 0.1},
        {{"", "14", "10"}, 0.1},
        {{"", "14", "11"}, 0.26},
        {{"", "14", "12"}, 0.23},
        {{"", "14", "13"}, 0.29},
        {{"", "14", "15"}, 0.15},
        {{"", "14", "16"}, 0.0},
        {{"", "15", "1"}, 0.04},
        {{"", "15", "2"}, 0.12},
        {{"", "15", "3"}, 0.12},
        {{"", "15", "4"}, 0.09},
        {{"", "15", "5"}, 0.12},
        {{"", "15", "6"}, 0.07},
        {{"", "15", "7"}, 0.09},
        {{"", "15", "8"}, 0.04},
        {{"", "15", "9"}, 0.1},
        {{"", "15", "10"}, 0.16},
        {{"", "15", "11"}, 0.18},
        {{"", "15", "12"}, 0.14},
        {{"", "15", "13"}, 0.06},
        {{"", "15", "14"}, 0.15},
        {{"", "15", "16"}, 0.0},
        {{"", "16", "1"}, 0.0},
        {{"", "16", "2"}, 0.0},
        {{"", "16", "3"}, 0.0},
        {{"", "16", "4"}, 0.0},
        {{"", "16", "5"}, 0.0},
        {{"", "16", "6"}, 0.0},
        {{"", "16", "7"}, 0.0},
        {{"", "16", "8"}, 0.0},
        {{"", "16", "9"}, 0.0},
        {{"", "16", "10"}, 0.0},
        {{"", "16", "11"}, 0.0},
        {{"", "16", "12"}, 0.0},
        {{"", "16", "13"}, 0.0},
        {{"", "16", "14"}, 0.0},
        {{"", "16", "15"}, 0.0}
    };

    // Equity intra-bucket correlations (exclude Residual and deal with it in the method - it is 0%)
    intraBucketCorrelation_[CrifRecord::RiskType::Equity] = {
        {{"1", "", ""}, 0.14},
        {{"2", "", ""}, 0.24},
        {{"3", "", ""}, 0.25},
        {{"4", "", ""}, 0.2},
        {{"5", "", ""}, 0.26},
        {{"6", "", ""}, 0.34},
        {{"7", "", ""}, 0.33},
        {{"8", "", ""}, 0.34},
        {{"9", "", ""}, 0.21},
        {{"10", "", ""}, 0.24},
        {{"11", "", ""}, 0.63}
    };

    // Commodity intra-bucket correlations
    intraBucketCorrelation_[CrifRecord::RiskType::Commodity] = {
        {{"1", "", ""}, 0.71},
        {{"2", "", ""}, 0.92},
        {{"3", "", ""}, 0.97},
        {{"4", "", ""}, 0.97},
        {{"5", "", ""}, 0.99},
        {{"6", "", ""}, 0.98},
        {{"7", "", ""}, 1.0},
        {{"8", "", ""}, 0.69},
        {{"9", "", ""}, 0.47},
        {{"10", "", ""}, 0.01},
        {{"11", "", ""}, 0.67},
        {{"12", "", ""}, 0.70},
        {{"13", "", ""}, 0.68},
        {{"14", "", ""}, 0.22},
        {{"15", "", ""}, 0.50},
        {{"16", "", ""}, 0.0 }
    };

    // Initialise the single, ad-hoc type, correlations
    xccyCorr_ = 0.0; // not a valid risk type
    infCorr_ = 0.33;
    infVolCorr_ = 0.0; // not a valid risk type
    irSubCurveCorr_ = 0.982;
    irInterCurrencyCorr_ = 0.27;
    crqResidualIntraCorr_ = 0.5;
    crqSameIntraCorr_ = 0.98;
    crqDiffIntraCorr_ = 0.55;
    crnqResidualIntraCorr_ = 0.5;
    crnqSameIntraCorr_ = 0.60;
    crnqDiffIntraCorr_ = 0.21;
    crnqInterCorr_ = 0.05;
    fxCorr_ = 0.5;
    basecorrCorr_ = 0.0; // not a valid risk type

    // clang-format on
}

} // namespace analytics
} // namespace ore
