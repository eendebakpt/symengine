#include <stdexcept>

#include <symengine/pow.h>
#include <symengine/add.h>
#include <symengine/mul.h>
#include <symengine/symbol.h>
#include <symengine/integer.h>
#include <symengine/rational.h>
#include <symengine/complex.h>
#include <symengine/constants.h>
#include <symengine/polynomial.h>

namespace SymEngine {

Pow::Pow(const RCP<const Basic> &base, const RCP<const Basic> &exp)
    : base_{base}, exp_{exp}
{
    SYMENGINE_ASSERT(is_canonical(base, exp))
}

bool Pow::is_canonical(const RCP<const Basic> &base, const RCP<const Basic> &exp)
{
    if (base == null) return false;
    if (exp == null) return false;
    // e.g. 0**x
    if (is_a<Integer>(*base) and rcp_static_cast<const Integer>(base)->is_zero())
        return false;
    // e.g. 1**x
    if (is_a<Integer>(*base) and rcp_static_cast<const Integer>(base)->is_one())
        return false;
    // e.g. x**0.0
    if (is_a_Number(*exp) and rcp_static_cast<const Number>(exp)->is_zero())
        return false;
    // e.g. x**1
    if (is_a<Integer>(*exp) and rcp_static_cast<const Integer>(exp)->is_one())
        return false;
    // e.g. 2**3, (2/3)**4
    if ((is_a<Integer>(*base) or is_a<Rational>(*base)) and is_a<Integer>(*exp))
        return false;
    // e.g. (x*y)**2, should rather be x**2*y**2
    if (is_a<Mul>(*base) and is_a<Integer>(*exp))
        return false;
    // e.g. (x**y)**2, should rather be x**(2*y)
    if (is_a<Pow>(*base) and is_a<Integer>(*exp))
        return false;
    // If exp is a rational, it should be between 0  and 1, i.e. we don't
    // allow things like 2**(-1/2) or 2**(3/2)
    if ((is_a<Rational>(*base) or is_a<Integer>(*base)) and
        is_a<Rational>(*exp) and
        (rcp_static_cast<const Rational>(exp)->i < 0 ||
        rcp_static_cast<const Rational>(exp)->i > 1))
        return false;
    // Purely Imaginary complex numbers with integral powers are expanded
    // e.g (2I)**3
    if (is_a<Complex>(*base) and rcp_static_cast<const Complex>(base)->is_re_zero() and
        is_a<Integer>(*exp))
        return false;
    // e.g. 0.5^2.0 should be represented as 0.25
    if(is_a_Number(*base) and !rcp_static_cast<const Number>(base)->is_exact() and
            is_a_Number(*exp) and !rcp_static_cast<const Number>(exp)->is_exact())
        return false;
    return true;
}

std::size_t Pow::__hash__() const
{
    std::size_t seed = POW;
    hash_combine<Basic>(seed, *base_);
    hash_combine<Basic>(seed, *exp_);
    return seed;
}

bool Pow::__eq__(const Basic &o) const
{
    if (is_a<Pow>(o) and
        eq(*base_, *(static_cast<const Pow &>(o).base_)) and
        eq(*exp_, *(static_cast<const Pow &>(o).exp_)))
            return true;

    return false;
}

int Pow::compare(const Basic &o) const
{
    SYMENGINE_ASSERT(is_a<Pow>(o))
    const Pow &s = static_cast<const Pow &>(o);
    int base_cmp = base_->__cmp__(*s.base_);
    if (base_cmp == 0)
        return exp_->__cmp__(*s.exp_);
    else
        return base_cmp;
}

RCP<const Basic> pow(const RCP<const Basic> &a, const RCP<const Basic> &b)
{
    if (is_a_Number(*b) and rcp_static_cast<const Number>(b)->is_zero()) {
        return pownum(rcp_static_cast<const Number>(b), zero);
    }
    if (eq(*b, *one)) return a;
    if (eq(*a, *zero)) return zero;
    if (eq(*a, *one)) return one;
    if (eq(*a, *minus_one)) {
        if (is_a<Integer>(*b)) {
            return is_a<Integer>(*div(b, integer(2))) ? one : minus_one;
        } else if (is_a<Rational>(*b) and
                    (rcp_static_cast<const Rational>(b)->i.get_num() == 1) and
                    (rcp_static_cast<const Rational>(b)->i.get_den() == 2)) {
            return I;
        }
    }

    if (is_a_Number(*a) and is_a_Number(*b)) {
        if (is_a<Integer>(*b)) {
            if (is_a<Rational>(*a)) {
                RCP<const Rational> exp_new = rcp_static_cast<const Rational>(a);
                return exp_new->powrat(*rcp_static_cast<const Integer>(b));
            } else if (is_a<Integer>(*a)) {
                RCP<const Integer> exp_new = rcp_static_cast<const Integer>(a);
                return exp_new->powint(*rcp_static_cast<const Integer>(b));
            } else if (is_a<Complex>(*a)) {
                RCP<const Complex> exp_new = rcp_static_cast<const Complex>(a);
                RCP<const Integer> pow_new = rcp_static_cast<const Integer>(b);
                RCP<const Number> res = exp_new->pow(*pow_new);
                return res;
            } else {
                return rcp_static_cast<const Number>(a)->pow(*rcp_static_cast<const Number>(b));
            }
        } else if (is_a<Rational>(*b)) {
            mpz_class q, r, num, den;
            num = rcp_static_cast<const Rational>(b)->i.get_num();
            den = rcp_static_cast<const Rational>(b)->i.get_den();

            if (num > den or num < 0) {
                mpz_fdiv_qr(q.get_mpz_t(), r.get_mpz_t(), num.get_mpz_t(),
                    den.get_mpz_t());
            } else {
                if (is_a_Number(*a) and !rcp_static_cast<const Number>(a)->is_exact()) {
                    return rcp_static_cast<const Number>(a)->pow(*rcp_static_cast<const Number>(b));
                }
                return make_rcp<const Pow>(a, b);
            }
            // Here we make the exponent postive and a fraction between
            // 0 and 1. We multiply numerator and denominator appropriately
            // to achieve this
            if (is_a<Rational>(*a)) {
                RCP<const Rational> exp_new = rcp_static_cast<const Rational>(a);
                RCP<const Basic> frac =
                    div(exp_new->powrat(Integer(q)), integer(exp_new->i.get_den()));
                RCP<const Basic> surds =
                    mul(make_rcp<const Pow>(integer(exp_new->i.get_num()), div(integer(r), integer(den))),
                        make_rcp<const Pow>(integer(exp_new->i.get_den()), sub(one, div(integer(r), integer(den)))));
                return mul(frac, surds);
            } else if (is_a<Integer>(*a)) {
                RCP<const Integer> exp_new = rcp_static_cast<const Integer>(a);
                RCP<const Number> frac = exp_new->powint(Integer(q));
                map_basic_basic surd;
                if ((exp_new->is_negative()) and (2 * r == den)) {
                    frac = mulnum(frac, I);
                    exp_new = exp_new->mulint(*minus_one);
                    // if exp_new is one, no need to add it to dict
                    if (exp_new->is_one())
                        return frac;
                    surd[exp_new] = div(integer(r), integer(den));
                } else {
                    surd[exp_new] = div(integer(r), integer(den));
                }
                return make_rcp<const Mul>(frac, std::move(surd));
            } else if (is_a<Complex>(*a)) {
                return make_rcp<const Pow>(a, b);
            } else {
                return rcp_static_cast<const Number>(a)->pow(*rcp_static_cast<const Number>(b));
            }
        } else if (is_a<Complex>(*b)) {
            return make_rcp<const Pow>(a, b);
        } else {
            return rcp_static_cast<const Number>(a)->pow(*rcp_static_cast<const Number>(b));
        }
    }
    if (is_a<Mul>(*a) and is_a_Number(*b)) {
        map_basic_basic d;
        RCP<const Number> coef = one;
        rcp_static_cast<const Mul>(a)->power_num(outArg(coef), d, rcp_static_cast<const Number>(b));
        return Mul::from_dict(coef, std::move(d));
    }
    if (is_a<Pow>(*a) and is_a<Integer>(*b)) {
        // Convert (x**y)**b = x**(b*y), where 'b' is an integer. This holds for
        // any complex 'x', 'y' and integer 'b'.
        RCP<const Pow> A = rcp_static_cast<const Pow>(a);
        return pow(A->base_, mul(A->exp_, b));
    }
    return make_rcp<const Pow>(a, b);
}

// This function can overflow, but it is fast.
// TODO: figure out condition for (m, n) when it overflows and raise an
// exception.
void multinomial_coefficients(int m, int n, map_vec_int &r)
{
    vec_int t;
    int j, tj, start, k;
    long long int v;
    if (m < 2)
        throw std::runtime_error("multinomial_coefficients: m >= 2 must hold.");
    if (n < 0)
        throw std::runtime_error("multinomial_coefficients: n >= 0 must hold.");
    t.assign(m, 0);
    t[0] = n;
    r[t] = 1;
    if (n == 0) return;
    j = 0;
    while (j < m - 1) {
        tj = t[j];
        if (j) {
            t[j] = 0;
            t[0] = tj;
        }
        if (tj > 1) {
            t[j+1] += 1;
            j = 0;
            start = 1;
            v = 0;
        } else {
            j += 1;
            start = j + 1;
            v = r[t];
            t[j] += 1;
        }
        for (k=start; k<m; k++) {
            if (t[k]) {
                t[k] -= 1;
                v += r[t];
                t[k] += 1;
            }
        }
        t[0] -= 1;
        r[t] = (v*tj) / (n-t[0]);
    }
}

// Slower, but returns exact (possibly large) integers (as mpz)
void multinomial_coefficients_mpz(int m, int n, map_vec_mpz &r)
{
    vec_int t;
    int j, tj, start, k;
    mpz_class v;
    if (m < 2)
        throw std::runtime_error("multinomial_coefficients: m >= 2 must hold.");
    if (n < 0)
        throw std::runtime_error("multinomial_coefficients: n >= 0 must hold.");
    t.assign(m, 0);
    t[0] = n;
    r[t] = 1;
    if (n == 0) return;
    j = 0;
    while (j < m - 1) {
        tj = t[j];
        if (j) {
            t[j] = 0;
            t[0] = tj;
        }
        if (tj > 1) {
            t[j+1] += 1;
            j = 0;
            start = 1;
            v = 0;
        } else {
            j += 1;
            start = j + 1;
            v = r[t];
            t[j] += 1;
        }
        for (k=start; k<m; k++) {
            if (t[k]) {
                t[k] -= 1;
                v += r[t];
                t[k] += 1;
            }
        }
        t[0] -= 1;
        r[t] = (v*tj) / (n-t[0]);
    }
}

RCP<const Basic> pow_expand(const RCP<const Pow> &self)
{
    RCP<const Basic> _base = expand(self->base_);
    bool negative_pow = false;

    if (is_a<Integer>(*self->exp_) and is_a<UnivariatePolynomial>(*_base)) {
        int q = rcp_static_cast<const Integer>(self->exp_)->as_int();
        RCP<const UnivariatePolynomial> p = rcp_static_cast<const UnivariatePolynomial>(_base);
        RCP<const UnivariatePolynomial> r = univariate_polynomial(p->var_, 0, {{0, 1}});
        while (q != 0) {
            if (q % 2 == 1) {
                r = mul_uni_poly(r, p);
                q--;
            }
            p = mul_uni_poly(p, p);
            q /= 2;
        }
        return r;
    }

    if (! is_a<Integer>(*self->exp_) or ! is_a<Add>(*_base)) {
        if (neq(*_base, *self->base_)) {
            return pow(_base, self->exp_);
        } else {
            return self;
        }
    }

    map_vec_mpz r;
    int n = rcp_static_cast<const Integer>(self->exp_)->as_int();
    if (n < 0) {
        n = -n;
        negative_pow = true;
    }
    RCP<const Add> base = rcp_static_cast<const Add>(_base);
    RCP<const Number> add_overall_coeff = zero;
    umap_basic_num base_dict = base->dict_;
    if (! (base->coef_->is_zero())) {
        // Add the numerical coefficient into the dictionary. This
        // allows a little bit easier treatment below.
        insert(base_dict, base->coef_, one);
    } else {
        if (base_dict.size() == 1) {
            // Eg: (0.0 + x * 5) ** 2 == 0.0 + (x * 5) ** 2
            return add(base->coef_, pow(mul(base_dict.begin()->first,
                           base_dict.begin()->second), self->exp_));
        }
        add_overall_coeff = base->coef_;
    }
    int m = base_dict.size();
    multinomial_coefficients_mpz(m, n, r);
    umap_basic_num rd;
    // This speeds up overall expansion. For example for the benchmark
    // (y + x + z + w)**60 it improves the timing from 135ms to 124ms.
#if defined(HAVE_SYMENGINE_RESERVE)
    rd.reserve(2*r.size());
#endif
    for (auto &p: r) {
        auto power = p.first.begin();
        auto i2 = base_dict.begin();
        map_basic_basic d;
        RCP<const Number> overall_coeff=one;
        for (; power != p.first.end(); ++power, ++i2) {
            if (*power > 0) {
                RCP<const Integer> exp = integer(*power);
                RCP<const Basic> base = i2->first;
                if (is_a<Integer>(*base)) {
                    imulnum(outArg(overall_coeff),
                        rcp_static_cast<const Number>(
                        rcp_static_cast<const Integer>(base)->powint(*exp)));
                } else if (is_a<Symbol>(*base)) {
                    Mul::dict_add_term(d, exp, base);
                } else {
                    RCP<const Basic> exp2, t, tmp;
                    tmp = pow(base, exp);
                    if (is_a<Mul>(*tmp)) {
                        for (auto &p: (rcp_static_cast<const Mul>(tmp))->dict_) {
                            Mul::dict_add_term_new(outArg(overall_coeff), d,
                                    p.second, p.first);
                        }
                        imulnum(outArg(overall_coeff), (rcp_static_cast<const Mul>(tmp))->coef_);
                    } else if (is_a_Number(*tmp)) {
                        imulnum(outArg(overall_coeff), rcp_static_cast<const Number>(tmp));
                    } else {
                        Mul::as_base_exp(tmp, outArg(exp2), outArg(t));
                        Mul::dict_add_term_new(outArg(overall_coeff), d, exp2, t);
                    }
                }
                if (!(i2->second->is_one())) {
                    if (is_a<Integer>(*(i2->second)) or is_a<Rational>(*(i2->second))) {
                        imulnum(outArg(overall_coeff),
                        pownum(i2->second,
                            rcp_static_cast<const Number>(exp)));
                    } else if (is_a<Complex>(*(i2->second))) {
                        RCP<const Number> tmp = rcp_static_cast<const Complex>(i2->second)->pow(*exp);
                        imulnum(outArg(overall_coeff), tmp);
                    }
                }
            }
        }
        RCP<const Basic> term = Mul::from_dict(overall_coeff, std::move(d));
        RCP<const Number> coef2 = integer(p.second);
        if (is_a_Number(*term)) {
            iaddnum(outArg(add_overall_coeff),
                mulnum(rcp_static_cast<const Number>(term), coef2));
        } else {
            if (is_a<Mul>(*term) and
                    !(rcp_static_cast<const Mul>(term)->coef_->is_one())) {
                // Tidy up things like {2x: 3} -> {x: 6}
                imulnum(outArg(coef2),
                        rcp_static_cast<const Mul>(term)->coef_);
                // We make a copy of the dict_:
                map_basic_basic d2 = rcp_static_cast<const Mul>(term)->dict_;
                term = Mul::from_dict(one, std::move(d2));
            }
            Add::dict_add_term(rd, coef2, term);
        }
    }
    RCP<const Basic> result = Add::from_dict(add_overall_coeff, std::move(rd));
    if (negative_pow) result = pow(result, minus_one);
    return result;
}

RCP<const Basic> Pow::diff(const RCP<const Symbol> &x) const
{
    if (is_a_Number(*exp_))
        return mul(mul(exp_, pow(base_, sub(exp_, one))), base_->diff(x));
    else
        return mul(pow(base_, exp_), mul(exp_, log(base_))->diff(x));
}

RCP<const Basic> Pow::subs(const map_basic_basic &subs_dict) const
{
    RCP<const Pow> self = rcp_from_this_cast<const Pow>();
    auto it = subs_dict.find(self);
    if (it != subs_dict.end())
        return it->second;
    RCP<const Basic> base_new = base_->subs(subs_dict);
    RCP<const Basic> exp_new = exp_->subs(subs_dict);
    if (base_new == base_ and exp_new == exp_)
        return self;
    else
        return pow(base_new, exp_new);
}

vec_basic Pow::get_args() const {
    return {base_, exp_};
}

RCP<const Basic> exp(const RCP<const Basic> &x)
{
    return pow(E, x);
}

Log::Log(const RCP<const Basic> &arg)
    : arg_{arg}
{
    SYMENGINE_ASSERT(is_canonical(arg))
}

bool Log::is_canonical(const RCP<const Basic> &arg)
{
    if (arg == null) return false;
    //  log(0)
    if (is_a<Integer>(*arg) and rcp_static_cast<const Integer>(arg)->is_zero())
        return false;
    //  log(1)
    if (is_a<Integer>(*arg) and rcp_static_cast<const Integer>(arg)->is_one())
        return false;
    // log(E)
    if (eq(*arg, *E))
        return false;
    // Currently not implemented, however should be expanded as `-ipi + log(-arg)`
    if (is_a_Number(*arg) and rcp_static_cast<const Number>(arg)->is_negative())
        return false;
    if (is_a_Number(*arg) and !rcp_static_cast<const Number>(arg)->is_exact())
        return false;
    // log(num/den) = log(num) - log(den)
    if (is_a<Rational>(*arg))
        return false;
    return true;
}

std::size_t Log::__hash__() const
{
    std::size_t seed = LOG;
    hash_combine<Basic>(seed, *arg_);
    return seed;
}

bool Log::__eq__(const Basic &o) const
{
    if (is_a<Log>(o) and
        eq(*arg_, *(static_cast<const Log &>(o).get_arg())))
            return true;

    return false;
}

int Log::compare(const Basic &o) const
{
    SYMENGINE_ASSERT(is_a<Log>(o))
    const Log &s = static_cast<const Log &>(o);
    return arg_->__cmp__(*s.get_arg());
}

RCP<const Basic> Log::subs(const map_basic_basic &subs_dict) const
{
    RCP<const Log> self = rcp_from_this_cast<const Log>();
    auto it = subs_dict.find(self);
    if (it != subs_dict.end())
        return it->second;
    RCP<const Basic> arg_new = arg_->subs(subs_dict);
    if (arg_new == arg_) {
        return self;
    } else {
        return log(arg_new);
    }
}

RCP<const Basic> Log::diff(const RCP<const Symbol> &x) const
{
    return mul(div(one, arg_), arg_->diff(x));
}

RCP<const Basic> log(const RCP<const Basic> &arg)
{
    if (eq(*arg, *zero)) {
        throw std::runtime_error("log(0) is complex infinity. Yet to be implemented");
    }
    if (eq(*arg, *one)) return zero;
    if (eq(*arg, *E)) return one;
    if (is_a_Number(*arg)) {
        RCP<const Number> _arg = rcp_static_cast<const Number>(arg);
        if (!_arg->is_exact()) {
            return _arg->get_eval().log(*_arg);
        } else if (_arg->is_negative()) {
            throw std::runtime_error("Imaginary Result. Yet to be implemented");
        }
    }
    if (is_a<Rational>(*arg)) {
        RCP<const Integer> num, den;
        get_num_den(static_cast<const Rational &>(*arg), outArg(num), outArg(den));
        return sub(log(num), log(den));
    }
    return make_rcp<const Log>(arg);
}

RCP<const Basic> log(const RCP<const Basic> &arg, const RCP<const Basic> &base)
{
    return div(log(arg), log(base));
}
} // SymEngine
