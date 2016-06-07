#include <symengine/symbol.h>
#include <symengine/polys/uintpoly_piranha.h>

#ifdef HAVE_SYMENGINE_PIRANHA

namespace SymEngine
{

UIntPolyPiranha::UIntPolyPiranha(const RCP<const Symbol> &var,
                             pintpoly &&dict)
    : UPolyBase(var, std::move(dict))
{
}

std::size_t UIntPolyPiranha::__hash__() const
{
    std::size_t seed = UINTPOLYPIRANHA;
    std::hash<std::string> hash_string;
    seed += poly_.hash();
    hash_string(var_->get_name());
    return seed;
}

int UIntPolyPiranha::compare(const Basic &o) const
{
    SYMENGINE_ASSERT(is_a<UIntPolyPiranha>(o))
    const UIntPolyPiranha &s = static_cast<const UIntPolyPiranha &>(o);
    int cmp = var_->compare(*s.var_);
    if (cmp != 0)
        return cmp;
    if (poly_ == s.poly_)
        return 0;
    return (poly_.hash() < s.poly_.hash()) ? -1 : 1;
}

RCP<const UIntPolyPiranha> UIntPolyPiranha::from_dict(const RCP<const Symbol> &var,
                                                  map_uint_mpz &&d)
{
	pintpoly p;
	for (auto &it: d)
		p.insert(pterm{it.second, pmonomial{it.first}});

    return make_rcp<const UIntPolyPiranha>(var, std::move(p));
}

RCP<const UIntPolyPiranha>
UIntPolyPiranha::from_vec(const RCP<const Symbol> &var,
                        const std::vector<integer_class> &v)
{
	pintpoly p;
    for (unsigned int i = 0; i <= v.size(); i++) {
        if (v[i] != integer_class(0)) {
            p.insert(pterm{v[i], pmonomial{i}});
        }
    }
    return make_rcp<const UIntPolyPiranha>(var, std::move(p));
}

}
#endif