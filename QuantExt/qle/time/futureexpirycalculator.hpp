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

/*! \file qle/time/futureexpirycalculator.hpp
    \brief Base class for classes that perform date calculations for future contracts
*/

#ifndef quantext_future_expiry_calculator_hpp
#define quantext_future_expiry_calculator_hpp

#include <ql/settings.hpp>
#include <ql/time/date.hpp>

namespace QuantExt {

//! Base class for classes that perform date calculations for future contracts
class FutureExpiryCalculator {
public:
    /*! Given a future contract's name, \p contractName, and a reference date, \p referenceDate, return the expiry 
        date of the next futures contract relative to the reference date.
        
        The \p includeExpiry parameter controls what happens when the \p referenceDate is equal to the next contract's
        expiry date. If \p includeExpiry is \c true, the contract's expiry date is returned. If \p includeExpiry is 
        \c false, the next succeeding contract's expiry is returned.
    */
    virtual QuantLib::Date nextExpiry(const std::string& contractName, bool includeExpiry = true,
        const QuantLib::Date& referenceDate = QuantLib::Date()) = 0;
};

}

#endif