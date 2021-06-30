//
// Class ParticleAttrib
//   Templated class for all particle attribute classes.
//
//   This templated class is used to represent a single particle attribute.
//   An attribute is one data element within a particle object, and is
//   stored as a Kokkos::View. This class stores the type information for the
//   attribute, and provides methods to create and destroy new items, and
//   to perform operations involving this attribute with others.
//
//   ParticleAttrib is the primary element involved in expressions for
//   particles (just as BareField is the primary element there).  This file
//   defines the necessary templated classes and functions to make
//   ParticleAttrib a capable expression-template participant.
//
// Copyright (c) 2020, Matthias Frey, Paul Scherrer Institut, Villigen PSI, Switzerland
// All rights reserved
//
// This file is part of IPPL.
//
// IPPL is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// You should have received a copy of the GNU General Public License
// along with IPPL. If not, see <https://www.gnu.org/licenses/>.
//
#include "Ippl.h"
#include "Communicate/DataTypes.h"
#include "Utility/IpplTimings.h"

namespace ippl {

    template<typename T, class... Properties>
    void ParticleAttrib<T, Properties...>::create(size_t n) {
        size_t current = this->size();
        if (current < particleCount + n) {
            this->resize((current + n) * 2);
            particleCount += n;
        }
    }


    template<typename T, class... Properties>
    void ParticleAttrib<T, Properties...>::destroy(boolean_view_type invalidIndex,
                                                   Kokkos::View<int*> newIndex, size_t localNum, size_t destroyNum) {
        Kokkos::parallel_for("ParticleAttrib::destroy() copy to temp",
                             localNum + destroyNum,
                             KOKKOS_CLASS_LAMBDA(const size_t i)
                             {
                                 if ( !invalidIndex(i) ) {
                                    temp(newIndex(i)) = dview_m(i);
                                 }
                             });
        Kokkos::fence();
        Kokkos::parallel_for("ParticleAttrib::destroy() copy from temp", localNum,
                             KOKKOS_CLASS_LAMBDA(const size_t i)
                             {
                                 dview_m(i) = temp(i);
                             });
        Kokkos::fence();
        particleCount = localNum;
    }

    template<typename T, class... Properties>
    void ParticleAttrib<T, Properties...>::sort(const Kokkos::View<int*>& deleteIndex, const Kokkos::View<int*>& keepIndex, size_t maxDeleteIndex, size_t destroyNum) {
        Kokkos::parallel_for("ParticleAttrib::sort()",
                             maxDeleteIndex,
                             KOKKOS_CLASS_LAMBDA(const size_t i)
                             {
                                 T tmp = dview_m(deleteIndex(i));
                                 dview_m(deleteIndex(i)) = dview_m(keepIndex(i));
                                 dview_m(keepIndex(i)) = tmp;
                             });
        particleCount -= destroyNum;
    }

    template<typename T, class... Properties>
    void ParticleAttrib<T, Properties...>::pack(void* buffer,
                                                const Kokkos::View<int*>& hash) const
    {
        using this_type = ParticleAttrib<T, Properties...>;
        this_type* buffer_p = static_cast<this_type*>(buffer);
        auto& view = buffer_p->dview_m;
        auto size = hash.extent(0);
        if(size > view.extent(0)) {
            Kokkos::resize(view, size);
        }

        Kokkos::parallel_for(
            "ParticleAttrib::pack()",
            size,
            KOKKOS_CLASS_LAMBDA(const size_t i) {
                view(i) = dview_m(hash(i));
        });
        Kokkos::fence();
    }


    template <typename T, class... Properties>
    void ParticleAttrib<T, Properties...>::unpack(void* buffer, int nrecvs) {
        using this_type = ParticleAttrib<T, Properties...>;
        this_type* buffer_p = static_cast<this_type*>(buffer);
        auto& view = buffer_p->dview_m;
        auto size = dview_m.extent(0);
        if(size < particleCount + (size_t)nrecvs) {
            this->resize((particleCount + nrecvs) * 2);
        }

        Kokkos::parallel_for(
            "ParticleAttrib::unpack()",
            nrecvs,
            KOKKOS_CLASS_LAMBDA(const size_t i) {
                dview_m(particleCount + i) = view(i);
        });
        Kokkos::fence();
        particleCount += nrecvs;
    }

    template<typename T, class... Properties>
    //KOKKOS_INLINE_FUNCTION
    ParticleAttrib<T, Properties...>&
    ParticleAttrib<T, Properties...>::operator=(T x)
    {
        Kokkos::parallel_for("ParticleAttrib::operator=()",
                             particleCount,
                             KOKKOS_CLASS_LAMBDA(const size_t i) {
                                 dview_m(i) = x;
                            });
        return *this;
    }


    template<typename T, class... Properties>
    template <typename E, size_t N>
    //KOKKOS_INLINE_FUNCTION
    ParticleAttrib<T, Properties...>&
    ParticleAttrib<T, Properties...>::operator=(detail::Expression<E, N> const& expr)
    {
        using capture_type = detail::CapturedExpression<E, N>;
        capture_type expr_ = reinterpret_cast<const capture_type&>(expr);

        Kokkos::parallel_for("ParticleAttrib::operator=()",
                             particleCount,
                             KOKKOS_CLASS_LAMBDA(const size_t i) {
                                 dview_m(i) = expr_(i);
                            });
        return *this;
    }


    template<typename T, class... Properties>
    template <unsigned Dim, class M, class C, class PT>
    void ParticleAttrib<T, Properties...>::scatter(Field<T,Dim,M,C>& f,
                                                   const ParticleAttrib< Vector<PT,Dim>, Properties... >& pp)
    const
    {
        static IpplTimings::TimerRef scatterTimer = IpplTimings::getTimer("Scatter");           
        IpplTimings::startTimer(scatterTimer);                                               
        typename Field<T, Dim, M, C>::view_type view = f.getView();

        const M& mesh = f.get_mesh();

        using vector_type = typename M::vector_type;
        using value_type  = typename ParticleAttrib<T, Properties...>::value_type;

        const vector_type& dx = mesh.getMeshSpacing();
        const vector_type& origin = mesh.getOrigin();
        const vector_type invdx = 1.0 / dx;

        const FieldLayout<Dim>& layout = f.getLayout(); 
        const NDIndex<Dim>& lDom = layout.getLocalNDIndex();
        const int nghost = f.getNghost();

        Kokkos::parallel_for(
            "ParticleAttrib::scatter",
            particleCount,
            KOKKOS_CLASS_LAMBDA(const size_t idx)
            {
                // find nearest grid point
                vector_type l = (pp(idx) - origin) * invdx + 0.5;
                Vector<int, Dim> index = l;
                Vector<double, Dim> whi = l - index;
                Vector<double, Dim> wlo = 1.0 - whi;

                const size_t i = index[0] - lDom[0].first() + nghost;
                const size_t j = index[1] - lDom[1].first() + nghost;
                const size_t k = index[2] - lDom[2].first() + nghost;


                // scatter
                const value_type& val = dview_m(idx);
                Kokkos::atomic_add(&view(i-1, j-1, k-1), wlo[0] * wlo[1] * wlo[2] * val);
                Kokkos::atomic_add(&view(i-1, j-1, k  ), wlo[0] * wlo[1] * whi[2] * val);
                Kokkos::atomic_add(&view(i-1, j,   k-1), wlo[0] * whi[1] * wlo[2] * val);
                Kokkos::atomic_add(&view(i-1, j,   k  ), wlo[0] * whi[1] * whi[2] * val);
                Kokkos::atomic_add(&view(i,   j-1, k-1), whi[0] * wlo[1] * wlo[2] * val);
                Kokkos::atomic_add(&view(i,   j-1, k  ), whi[0] * wlo[1] * whi[2] * val);
                Kokkos::atomic_add(&view(i,   j,   k-1), whi[0] * whi[1] * wlo[2] * val);
                Kokkos::atomic_add(&view(i,   j,   k  ), whi[0] * whi[1] * whi[2] * val);
            }
        );
        IpplTimings::stopTimer(scatterTimer);
            
        static IpplTimings::TimerRef accumulateHaloTimer = IpplTimings::getTimer("AccumulateHalo");           
        IpplTimings::startTimer(accumulateHaloTimer);                                               
        f.accumulateHalo();
        IpplTimings::stopTimer(accumulateHaloTimer);                                               
    }


    template<typename T, class... Properties>
    template <unsigned Dim, class M, class C, typename P2>
    void ParticleAttrib<T, Properties...>::gather(Field<T, Dim, M, C>& f,
                                                  const ParticleAttrib<Vector<P2, Dim>, Properties...>& pp)
    {

        static IpplTimings::TimerRef fillHaloTimer = IpplTimings::getTimer("FillHalo");           
        IpplTimings::startTimer(fillHaloTimer);                                               
        f.fillHalo();
        IpplTimings::stopTimer(fillHaloTimer);                                               

        static IpplTimings::TimerRef gatherTimer = IpplTimings::getTimer("Gather");           
        IpplTimings::startTimer(gatherTimer);                                               
        const typename Field<T, Dim, M, C>::view_type view = f.getView();

        const M& mesh = f.get_mesh();

        using vector_type = typename M::vector_type;
        using value_type  = typename ParticleAttrib<T, Properties...>::value_type;

        const vector_type& dx = mesh.getMeshSpacing();
        const vector_type& origin = mesh.getOrigin();
        const vector_type invdx = 1.0 / dx;

        const FieldLayout<Dim>& layout = f.getLayout(); 
        const NDIndex<Dim>& lDom = layout.getLocalNDIndex();
        const int nghost = f.getNghost();

        Kokkos::parallel_for(
            "ParticleAttrib::gather",
            particleCount,
            KOKKOS_CLASS_LAMBDA(const size_t idx)
            {
                // find nearest grid point
                vector_type l = (pp(idx) - origin) * invdx + 0.5;
                Vector<int, Dim> index = l;
                Vector<double, Dim> whi = l - index;
                Vector<double, Dim> wlo = 1.0 - whi;

                const size_t i = index[0] - lDom[0].first() + nghost;
                const size_t j = index[1] - lDom[1].first() + nghost;
                const size_t k = index[2] - lDom[2].first() + nghost;

                // gather
                value_type& val = dview_m(idx);
                val = wlo[0] * wlo[1] * wlo[2] * view(i-1, j-1, k-1)
                    + wlo[0] * wlo[1] * whi[2] * view(i-1, j-1, k  )
                    + wlo[0] * whi[1] * wlo[2] * view(i-1, j,   k-1)
                    + wlo[0] * whi[1] * whi[2] * view(i-1, j,   k  )
                    + whi[0] * wlo[1] * wlo[2] * view(i,   j-1, k-1)
                    + whi[0] * wlo[1] * whi[2] * view(i,   j-1, k  )
                    + whi[0] * whi[1] * wlo[2] * view(i,   j,   k-1)
                    + whi[0] * whi[1] * whi[2] * view(i,   j,   k  );
            }
        );
        IpplTimings::stopTimer(gatherTimer);                                               
    }



    /*
     * Non-class function
     *
     */


    template<typename P1, unsigned Dim, class M, class C, typename P2, class... Properties>
    inline
    void scatter(const ParticleAttrib<P1, Properties...>& attrib, Field<P1, Dim, M, C>& f,
                 const ParticleAttrib<Vector<P2, Dim>, Properties...>& pp)
    {
        attrib.scatter(f, pp);
    }


    template<typename P1, unsigned Dim, class M, class C, typename P2, class... Properties>
    inline
    void gather(ParticleAttrib<P1, Properties...>& attrib, Field<P1, Dim, M, C>& f,
                const ParticleAttrib<Vector<P2, Dim>, Properties...>& pp)
    {
        attrib.gather(f, pp);
    }

    #define DefineParticleReduction(fun, name, op, MPI_Op)                                                   \
    template<typename T, class... Properties>                                                                \
    T ParticleAttrib<T, Properties...>::name() {                                                             \
        T temp = 0.0;                                                                                        \
        Kokkos::parallel_reduce("fun", particleCount,                                                        \
                               KOKKOS_CLASS_LAMBDA(const size_t i, T& valL) {                                \
                                    T myVal = dview_m(i);                                                    \
                                    op;                                                                      \
                               }, Kokkos::fun<T>(temp));                                                     \
        T globaltemp = 0.0;                                                                                  \
        MPI_Datatype type = get_mpi_datatype<T>(temp);                                                       \
        MPI_Allreduce(&temp, &globaltemp, 1, type, MPI_Op, Ippl::getComm());                                 \
        return globaltemp;                                                                                   \
    }

    DefineParticleReduction(Sum,  sum,  valL += myVal, MPI_SUM)
    DefineParticleReduction(Max,  max,  if(myVal > valL) valL = myVal, MPI_MAX)
    DefineParticleReduction(Min,  min,  if(myVal < valL) valL = myVal, MPI_MIN)
    DefineParticleReduction(Prod, prod, valL *= myVal, MPI_PROD)
}
