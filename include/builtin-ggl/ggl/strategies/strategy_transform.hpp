// Generic Geometry Library
//
// Copyright Barend Gehrels 1995-2009, Geodan Holding B.V. Amsterdam, the Netherlands.
// Copyright Bruno Lalande 2008, 2009
// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef GGL_STRATEGY_TRANSFORM_HPP
#define GGL_STRATEGY_TRANSFORM_HPP

#include <cstddef>
#include <cmath>
#include <functional>

#include <boost/numeric/conversion/cast.hpp>

#include <ggl/arithmetic/arithmetic.hpp>
#include <ggl/core/access.hpp>
#include <ggl/core/coordinate_dimension.hpp>
#include <ggl/core/concepts/point_concept.hpp>
#include <ggl/util/copy.hpp>
#include <ggl/util/math.hpp>
#include <ggl/util/select_coordinate_type.hpp>

namespace ggl
{

namespace strategy { namespace transform {

#ifndef DOXYGEN_NO_DETAIL
namespace detail
{

template
<
    typename Src, typename Dst,
    std::size_t D, std::size_t N,
    template <typename> class F
>
struct transform_coordinates
{
    static inline void transform(const Src& source, Dst& dest, double value)
    {
        typedef typename select_coordinate_type<Src, Dst>::type coordinate_type;

        F<coordinate_type> function;
        set<D>(dest, boost::numeric_cast<coordinate_type>(function(get<D>(source), value)));
        transform_coordinates<Src, Dst, D + 1, N, F>::transform(source, dest, value);
    }
};

template
<
    typename Src, typename Dst,
    std::size_t N,
    template <typename> class F
>
struct transform_coordinates<Src, Dst, N, N, F>
{
    static inline void transform(const Src& source, Dst& dest, double value)
    {
    }
};

} // namespace detail
#endif // DOXYGEN_NO_DETAIL


/*!
    \brief Transformation strategy to copy one point to another using assignment operator
    \ingroup transform
    \tparam P point type
 */
template <typename P>
struct copy_direct
{
    inline bool operator()(const P& p1, P& p2) const
    {
        p2 = p1;
        return true;
    }
};

/*!
    \brief Transformation strategy to do copy a point, copying per coordinate.
    \ingroup transform
    \tparam P1 first point type
    \tparam P2 second point type
 */
template <typename P1, typename P2>
struct copy_per_coordinate
{
    inline bool operator()(P1 const& p1, P2& p2) const
    {
        // Defensive check, dimensions are equal, selected by specialization
        assert_dimension_equal<P1, P2>();

        copy_coordinates(p1, p2);
        return true;
    }
};


/*!
    \brief Transformation strategy to go from degree to radian and back
    \ingroup transform
    \tparam P1 first point type
    \tparam P2 second point type
    \tparam F additional functor to divide or multiply with d2r
 */
template <typename P1, typename P2, template <typename> class F>
struct degree_radian_vv
{
    inline bool operator()(P1 const& p1, P2& p2) const
    {
        // Spherical coordinates always have 2 coordinates measured in angles
        // The optional third one is distance/height, provided in another strategy
        // Polar coordinates having one angle, will be also in another strategy
        assert_dimension<P1, 2>();
        assert_dimension<P2, 2>();

        detail::transform_coordinates<P1, P2, 0, 2, F>::transform(p1, p2, math::d2r);
        return true;
    }
};

template <typename P1, typename P2, template <typename> class F>
struct degree_radian_vv_3
{
    inline bool operator()(P1 const& p1, P2& p2) const
    {
        assert_dimension<P1, 3>();
        assert_dimension<P2, 3>();

        detail::transform_coordinates<P1, P2, 0, 2, F>::transform(p1, p2, math::d2r);
        // Copy height or other third dimension
        set<2>(p2, get<2>(p1));
        return true;
    }
};


#ifndef DOXYGEN_NO_DETAIL
namespace detail
{

    /// Helper function for conversion, phi/theta are in radians
    template <typename P>
    inline void spherical_to_cartesian(double phi, double theta, double r, P& p)
    {
        assert_dimension<P, 3>();

        // http://en.wikipedia.org/wiki/List_of_canonical_coordinate_transformations#From_spherical_coordinates
        // Phi = first, theta is second, r is third, see documentation on cs::spherical
        double const sin_theta = std::sin(theta);
        set<0>(p, r * sin_theta * std::cos(phi));
        set<1>(p, r * sin_theta * std::sin(phi));
        set<2>(p, r * std::cos(theta));
    }

    /// Helper function for conversion
    template <typename P>
    inline bool cartesian_to_spherical2(double x, double y, double z, P& p)
    {
        assert_dimension<P, 2>();

        // http://en.wikipedia.org/wiki/List_of_canonical_coordinate_transformations#From_Cartesian_coordinates

        // TODO: MAYBE ONLY IF TO BE CHECKED?
        double const r = std::sqrt(x * x + y * y + z * z);

        // Unit sphere, r should be 1
        typedef typename coordinate_type<P>::type coordinate_type;
        if (std::abs(r - 1.0) > std::numeric_limits<coordinate_type>::epsilon())
        {
            return false;
        }
        // end todo

        set_from_radian<0>(p, std::atan2(y, x));
        set_from_radian<1>(p, std::acos(z));
        return true;
    }

    template <typename P>
    inline bool cartesian_to_spherical3(double x, double y, double z, P& p)
    {
        assert_dimension<P, 3>();

        // http://en.wikipedia.org/wiki/List_of_canonical_coordinate_transformations#From_Cartesian_coordinates
        double const r = std::sqrt(x * x + y * y + z * z);
        set<2>(p, r);
        set_from_radian<0>(p, std::atan2(y, x));
        if (r > 0.0)
        {
            set_from_radian<1>(p, std::acos(z / r));
            return true;
        }
        return false;
    }

} // namespace detail
#endif // DOXYGEN_NO_DETAIL


/*!
    \brief Transformation strategy for 2D spherical (phi,theta) to 3D cartesian (x,y,z)
    \details on Unit sphere
    \ingroup transform
    \tparam P1 first point type
    \tparam P2 second point type
 */
template <typename P1, typename P2>
struct from_spherical_2_to_cartesian_3
{
    inline bool operator()(P1 const& p1, P2& p2) const
    {
        assert_dimension<P1, 2>();
        detail::spherical_to_cartesian(get_as_radian<0>(p1), get_as_radian<1>(p1), 1.0, p2);
        return true;
    }
};

/*!
    \brief Transformation strategy for 3D spherical (phi,theta,r) to 3D cartesian (x,y,z)
    \ingroup transform
    \tparam P1 first point type
    \tparam P2 second point type
 */
template <typename P1, typename P2>
struct from_spherical_3_to_cartesian_3
{
    inline bool operator()(P1 const& p1, P2& p2) const
    {
        assert_dimension<P1, 3>();
        detail::spherical_to_cartesian(
                    get_as_radian<0>(p1), get_as_radian<1>(p1), get<2>(p1), p2);
        return true;
    }
};

/*!
    \brief Transformation strategy for 3D cartesian (x,y,z) to 2D spherical (phi,theta)
    \details on Unit sphere
    \ingroup transform
    \tparam P1 first point type
    \tparam P2 second point type
    \note If x,y,z point is not lying on unit sphere, transformation will return false
 */
template <typename P1, typename P2>
struct from_cartesian_3_to_spherical_2
{
    inline bool operator()(P1 const& p1, P2& p2) const
    {
        assert_dimension<P1, 3>();
        return detail::cartesian_to_spherical2(get<0>(p1), get<1>(p1), get<2>(p1), p2);
    }
};


/*!
    \brief Transformation strategy for 3D cartesian (x,y,z) to 3D spherical (phi,theta,r)
    \ingroup transform
    \tparam P1 first point type
    \tparam P2 second point type
 */
template <typename P1, typename P2>
struct from_cartesian_3_to_spherical_3
{
    inline bool operator()(P1 const& p1, P2& p2) const
    {
        assert_dimension<P1, 3>();
        return detail::cartesian_to_spherical3(get<0>(p1), get<1>(p1), get<2>(p1), p2);
    }
};

}} // namespace strategy::transform


#ifndef DOXYGEN_NO_STRATEGY_SPECIALIZATIONS

/// Specialization for same coordinate system family, same system, same dimension, same point type, can be copied
template <typename CoordSysTag, typename CoordSys, std::size_t D, typename P>
struct strategy_transform<CoordSysTag, CoordSysTag, CoordSys, CoordSys, D, D, P, P>
{
    typedef strategy::transform::copy_direct<P> type;
};

/// Specialization for same coordinate system family and system, same dimension, different point type, copy per coordinate
template <typename CoordSysTag, typename CoordSys, std::size_t D, typename P1, typename P2>
struct strategy_transform<CoordSysTag, CoordSysTag, CoordSys, CoordSys, D, D, P1, P2>
{
    typedef strategy::transform::copy_per_coordinate<P1, P2> type;
};

/// Specialization to convert from degree to radian for any coordinate system / point type combination
template <typename CoordSysTag, template<typename> class CoordSys, typename P1, typename P2>
struct strategy_transform<CoordSysTag, CoordSysTag, CoordSys<degree>, CoordSys<radian>, 2, 2, P1, P2>
{
    typedef strategy::transform::degree_radian_vv<P1, P2, std::multiplies> type;
};

/// Specialization to convert from radian to degree for any coordinate system / point type combination
template <typename CoordSysTag, template<typename> class CoordSys, typename P1, typename P2>
struct strategy_transform<CoordSysTag, CoordSysTag, CoordSys<radian>, CoordSys<degree>, 2, 2, P1, P2>
{
    typedef strategy::transform::degree_radian_vv<P1, P2, std::divides> type;
};


/// Specialization degree->radian in 3D
template <typename CoordSysTag, template<typename> class CoordSys, typename P1, typename P2>
struct strategy_transform<CoordSysTag, CoordSysTag, CoordSys<degree>, CoordSys<radian>, 3, 3, P1, P2>
{
    typedef strategy::transform::degree_radian_vv_3<P1, P2, std::multiplies> type;
};

/// Specialization radian->degree in 3D
template <typename CoordSysTag, template<typename> class CoordSys, typename P1, typename P2>
struct strategy_transform<CoordSysTag, CoordSysTag, CoordSys<radian>, CoordSys<degree>, 3, 3, P1, P2>
{
    typedef strategy::transform::degree_radian_vv_3<P1, P2, std::divides> type;
};

/// Specialization to convert from unit sphere(phi,theta) to XYZ
template <typename CoordSys1, typename CoordSys2, typename P1, typename P2>
struct strategy_transform<spherical_tag, cartesian_tag, CoordSys1, CoordSys2, 2, 3, P1, P2>
{
    typedef strategy::transform::from_spherical_2_to_cartesian_3<P1, P2> type;
};

/// Specialization to convert from sphere(phi,theta,r) to XYZ
template <typename CoordSys1, typename CoordSys2, typename P1, typename P2>
struct strategy_transform<spherical_tag, cartesian_tag, CoordSys1, CoordSys2, 3, 3, P1, P2>
{
    typedef strategy::transform::from_spherical_3_to_cartesian_3<P1, P2> type;
};

/// Specialization to convert from XYZ to unit sphere(phi,theta)
template <typename CoordSys1, typename CoordSys2, typename P1, typename P2>
struct strategy_transform<cartesian_tag, spherical_tag, CoordSys1, CoordSys2, 3, 2, P1, P2>
{
    typedef strategy::transform::from_cartesian_3_to_spherical_2<P1, P2> type;
};

/// Specialization to convert from XYZ to sphere(phi,theta,r)
template <typename CoordSys1, typename CoordSys2, typename P1, typename P2>
struct strategy_transform<cartesian_tag, spherical_tag, CoordSys1, CoordSys2, 3, 3, P1, P2>
{
    typedef strategy::transform::from_cartesian_3_to_spherical_3<P1, P2> type;
};

#endif // DOXYGEN_NO_STRATEGY_SPECIALIZATIONS

} // namespace ggl

#endif // GGL_STRATEGY_TRANSFORM_HPP
