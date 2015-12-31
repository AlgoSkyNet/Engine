/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2015 Quaternion Risk Management

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include "analyticlgmswaptionengine.hpp"
#include "utilities.hpp"

#include <qle/models/all.hpp>
#include <qle/pricingengines/all.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;
using namespace QuantExt;

using boost::unit_test_framework::test_suite;

void AnalyticLgmSwaptionEngineTest::testMonoCurve() {

    BOOST_TEST_MESSAGE(
        "Testing analytic LGM swaption engine in mono curve setup...");
}

void AnalyticLgmSwaptionEngineTest::testDualCurve() {

    BOOST_TEST_MESSAGE(
        "Testing analytic LGM swaption engine in dual curve setup...");
}

test_suite *AnalyticLgmSwaptionEngineTest::suite() {
    test_suite *suite = BOOST_TEST_SUITE("Analytic LGM swaption engine tests");
    suite->add(
        QUANTEXT_TEST_CASE(&AnalyticLgmSwaptionEngineTest::testMonoCurve));
    suite->add(
        QUANTEXT_TEST_CASE(&AnalyticLgmSwaptionEngineTest::testDualCurve));
    return suite;
}
