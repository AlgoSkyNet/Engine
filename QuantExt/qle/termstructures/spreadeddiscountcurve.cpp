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

#include <qle/termstructures/spreadeddiscountcurve.hpp>

namespace QuantExt {

SpreadedDiscountCurve::SpreadedDiscountCurve(const Handle<YieldTermStructure>& referenceCurve,
                                             const std::vector<Time>& times, const std::vector<Handle<Quote>>& quotes)
    : YieldTermStructure(referenceCurve->dayCounter()), referenceCurve_(referenceCurve), times_(times), quotes_(quotes),
      data_(times_.size(), 1.0) {
    QL_REQUIRE(times_.size() > 1, "SpreadedDiscountCurve: at least two times required");
    QL_REQUIRE(times_.size() == quotes.size(), "SpreadedDiscountCurve: size of time and quote vectors do not match");
    QL_REQUIRE(times_[0] == 0.0, "SpreadedDiscountCurve: first time must be 0, got " << times_[0]);
    for (Size i = 0; i < quotes.size(); ++i) {
        registerWith(quotes_[i]);
    }
    interpolation_ = boost::make_shared<LogLinearInterpolation>(times_.begin(), times_.end(), data_.begin());
    registerWith(referenceCurve_);
}

Date SpreadedDiscountCurve::maxDate() const { return referenceCurve_->maxDate(); }

void SpreadedDiscountCurve::update() {
    LazyObject::update();
    TermStructure::update();
}

const Date& SpreadedDiscountCurve::referenceDate() const { return referenceCurve_->referenceDate(); }

Calendar SpreadedDiscountCurve::calendar() const { return referenceCurve_->calendar(); }

Natural SpreadedDiscountCurve::settlementDays() const { return referenceCurve_->settlementDays(); }

void SpreadedDiscountCurve::performCalculations() const {
    for (Size i = 0; i < times_.size(); ++i) {
        QL_REQUIRE(!quotes_[i].empty(), "SpreadedDiscountCurve: quote at index " << i << " is empty");
        data_[i] = quotes_[i]->value();
        QL_REQUIRE(data_[i] > 0, "SpreadedDiscountCurve: invalid value " << data_[i] << " at index " << i);
    }
    interpolation_->update();
}

DiscountFactor SpreadedDiscountCurve::discountImpl(Time t) const {
    calculate();
    if (t <= this->times_.back())
        return referenceCurve_->discount(t) * (*interpolation_)(t, true);
    // flat fwd extrapolation
    Time tMax = this->times_.back();
    DiscountFactor dMax = this->data_.back();
    Rate instFwdMax = -(*interpolation_).derivative(tMax) / dMax;
    return referenceCurve_->discount(t) * dMax * std::exp(-instFwdMax * (t - tMax));
}

} // namespace QuantExt
